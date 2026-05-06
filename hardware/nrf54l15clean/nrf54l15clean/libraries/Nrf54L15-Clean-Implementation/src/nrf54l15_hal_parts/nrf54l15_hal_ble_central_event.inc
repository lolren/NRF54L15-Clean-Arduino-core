bool BleRadio::pollCentralConnectionEvent(BleConnectionEvent* event,
                                          uint32_t spinLimit) {
  if (!initialized_ || radio_ == nullptr || !connected_ ||
      connectionRole_ != BleConnectionRole::kCentral) {
    return false;
  }

  uint32_t nowUs = bleTimingUs();
  const uint32_t txAnchorUs = connectionNextEventUs_;
  {
    const uint16_t unsyncedEventCount =
        (connectionMissedEventCount_ < 0xFFFFU)
            ? static_cast<uint16_t>(connectionMissedEventCount_ + 1U)
            : 0xFFFFU;
    uint32_t lateAllowanceUs = bleConnectionRxListenUs(connectionIntervalUnits_,
                                                       connectionSca_,
                                                       unsyncedEventCount);
    lateAllowanceUs =
        (lateAllowanceUs > kBleConnRxStartLeadUs)
            ? static_cast<uint32_t>(lateAllowanceUs - kBleConnRxStartLeadUs)
            : 0U;
    // If we are already beyond the peer's receive window for this event,
    // skip it entirely. Advancing to the next event/channel and transmitting
    // or listening immediately in the same call can lock both sides onto the
    // right channel at the wrong time until supervision timeout.
    if (timeReachedUs(nowUs, txAnchorUs + lateAllowanceUs)) {
      ++encDebug_.connLatePollCount;
      if (connectionMissedEventCount_ < 0xFFFFU) {
        ++connectionMissedEventCount_;
      }
      noteConnectionMissedEvent(&encDebug_, connectionMissedEventCount_);
      {
        const uint32_t intervalUs =
            (connectionIntervalUnits_ > 0U)
                ? (static_cast<uint32_t>(connectionIntervalUnits_) * 1250UL)
                : 1250UL;
        const uint32_t timeoutUs =
            static_cast<uint32_t>(connectionTimeoutUnits_) * 10000UL;
        uint32_t supervisionEvents = timeoutUs / intervalUs;
        if (supervisionEvents < 1U) {
          supervisionEvents = 1U;
        }
        if (connectionMissedEventCount_ >= supervisionEvents) {
          if (event != nullptr) {
            event->terminateInd = true;
          }
          emitBleTrace("SUPERVISION_TIMEOUT");
          disconnectWithReason(BleDisconnectReason::kSupervisionTimeout, 0U,
                               spinLimit / 2U + 1U);
          return true;
        }
      }
      updateNextConnectionEventTime();
      emitBleTrace("CENTRAL_LATE_SKIP");
      return false;
    }
  }
  if (!timeReachedUs(nowUs, txAnchorUs)) {
    const uint32_t prewaitStartUs =
        (txAnchorUs > kBleConnAnchorPrewaitUs)
            ? (txAnchorUs - kBleConnAnchorPrewaitUs)
            : txAnchorUs;
    if (!timeReachedUs(nowUs, prewaitStartUs)) {
      uint32_t waitGuard = spinLimit;
      while (waitGuard-- > 0U && !timeReachedUs(nowUs, prewaitStartUs)) {
        nowUs = bleTimingUs();
      }
      if (!timeReachedUs(nowUs, prewaitStartUs)) {
        return false;
      }
    }
    while (!timeReachedUs(nowUs, txAnchorUs)) {
      nowUs = bleTimingUs();
    }
  }

  const uint32_t currentEventAnchorUs = connectionNextEventUs_;
  updateNextConnectionEventTime();

  const uint16_t currentEventCounter =
      (connectionEventCounter_ > 0U)
          ? static_cast<uint16_t>(connectionEventCounter_ - 1U)
          : 0U;
  (void)applyPendingConnectionPhyUpdateAtInstant(currentEventCounter);
  applyPendingConnectionUpdateAtInstant(currentEventCounter, currentEventAnchorUs);
  const uint8_t dataChannel = selectNextDataChannel(false);
  if (!setDataChannel(dataChannel)) {
    return false;
  }

  if (event != nullptr) {
    event->eventStarted = true;
    event->eventCounter = currentEventCounter;
    event->dataChannel = dataChannel;
    event->txPhy = connectionCurrentTxPhy_;
    event->rxPhy = connectionCurrentRxPhy_;
  }

  const bool canSendNewPayloadThisEvent =
      !connectionTxHistoryValid_ || connectionFreshTxAllowed_;
  // Do not replace an earlier empty ACK with a fresh queued central request.
  // Reusing the same SN with a different ATT/L2CAP payload is fragile during
  // coded-PHY setup and under duplex traffic, and can make the peer treat the
  // request as a duplicate or miss it entirely. Wait for a fresh TX state
  // before promoting any queued central request.
  const bool promotePendingTxAfterEmptyAck = false;
  uint8_t txLlid = 0x01U;
  uint8_t txLength = 0U;
  if (canSendNewPayloadThisEvent || promotePendingTxAfterEmptyAck) {
    if (connectionPendingTxValid_) {
      txLlid = connectionPendingTxLlid_;
      txLength = connectionPendingTxLength_;
      if (txLength > 0U) {
        memcpy(connectionTxPayload_, connectionPendingTxPayload_, txLength);
      }
      connectionPendingTxLlid_ = 0x01U;
      connectionPendingTxLength_ = 0U;
      connectionPendingTxValid_ = false;
    }
    connectionLastTxLlid_ = txLlid;
    connectionLastTxLength_ = txLength;
    connectionLastTxPlainLlid_ = txLlid;
    connectionLastTxPlainLength_ = txLength;
    memset(connectionLastTxPlainPayload_, 0, sizeof(connectionLastTxPlainPayload_));
    if (txLength > 0U) {
      memcpy(connectionLastTxPlainPayload_, connectionTxPayload_, txLength);
    }
  } else {
    txLlid = connectionLastTxLlid_;
    txLength = connectionLastTxLength_;
    if (txLength > 0U) {
      memcpy(connectionTxPayload_, connectionLastTxPlainPayload_, txLength);
    }
  }

  const bool txHasFollowingPayload =
      (canSendNewPayloadThisEvent || promotePendingTxAfterEmptyAck) &&
      (connectionPendingTxValid_ ||
       (connectionCustomNotificationQueueCount_ > 0U));
  txPacket_[0] = static_cast<uint8_t>((txLlid & 0x03U) |
                                      ((connectionExpectedRxSn_ & 0x01U) << 2U) |
                                      ((connectionTxSn_ & 0x01U) << 3U) |
                                      (txHasFollowingPayload ? 0x10U : 0U));
  const bool reuseEncryptedTx =
      !canSendNewPayloadThisEvent && connectionLastTxWasEncrypted_;
  bool txBuiltFreshEncrypted = false;
  uint64_t txEncryptedCounter = connectionEncTxCounter_;
  if (reuseEncryptedTx) {
    txLength = connectionLastTxEncryptedLength_;
    txPacket_[1] = txLength;
    if (txLength > 0U) {
      memcpy(&txPacket_[2], connectionLastTxEncryptedPayload_, txLength);
    }
  } else if (connectionEncSessionValid_ && connectionEncTxEnabled_) {
    uint8_t encryptedLen = 0U;
    if (bleCcmEncryptPayload(connectionEncSessionKey_, connectionEncIv_,
                             txEncryptedCounter, connectionEncTxDirection_,
                             txPacket_[0],
                             (txLength > 0U) ? connectionTxPayload_ : nullptr,
                             txLength, connectionLastTxEncryptedPayload_,
                             &encryptedLen)) {
      connectionLastTxWasEncrypted_ = true;
      connectionLastTxEncryptedLength_ = encryptedLen;
      txLength = encryptedLen;
      txPacket_[1] = txLength;
      if (txLength > 0U) {
        memcpy(&txPacket_[2], connectionLastTxEncryptedPayload_, txLength);
      }
      txBuiltFreshEncrypted = true;
    } else {
      connectionLastTxWasEncrypted_ = false;
      connectionLastTxEncryptedLength_ = 0U;
      txPacket_[1] = txLength;
      if (txLength > 0U) {
        memcpy(&txPacket_[2], connectionTxPayload_, txLength);
      }
      emitBleTrace("ENC_TX_BUILD_FAIL");
    }
  } else {
    connectionLastTxWasEncrypted_ = false;
    connectionLastTxEncryptedLength_ = 0U;
    txPacket_[1] = txLength;
    if (txLength > 0U) {
      memcpy(&txPacket_[2], connectionTxPayload_, txLength);
    }
  }

  const uint8_t txNesnBit = static_cast<uint8_t>((txPacket_[0] >> 2U) & 0x01U);
  const uint8_t txSnBit = static_cast<uint8_t>((txPacket_[0] >> 3U) & 0x01U);

  clearRadioCoreEvents(radio_);
  const size_t txCopyLen = 2U + static_cast<size_t>(txLength);
  const size_t copyLen = (txCopyLen <= sizeof(rxPacket_)) ? txCopyLen : sizeof(rxPacket_);
  memcpy(rxPacket_, txPacket_, copyLen);
  radio_->PACKETPTR =
      static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
  radio_->SHORTS =
      ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
       RADIO_SHORTS_TXREADY_START_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk) |
      ((RADIO_SHORTS_DISABLED_RXEN_Enabled << RADIO_SHORTS_DISABLED_RXEN_Pos) &
       RADIO_SHORTS_DISABLED_RXEN_Msk) |
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk);

  uint32_t txTriggerNowUs = bleTimingUs();
  while (!timeReachedUs(txTriggerNowUs, txAnchorUs)) {
    txTriggerNowUs = bleTimingUs();
  }
  radio_->TASKS_TXEN = RADIO_TASKS_TXEN_TASKS_TXEN_Trigger;

  const uint32_t txEndBudgetUs =
      bleConnectionTxEndBudgetUsOnPhy(txLength, connectionCurrentTxPhy_);
  const bool txEndSeen =
      waitRadioEndBudgeted(radio_, txEndBudgetUs, spinLimit / 2U + 1U);
  bool txOk = false;
  if (txEndSeen) {
    radio_->EVENTS_END = 0U;
    radio_->EVENTS_PHYEND = 0U;
    txOk = waitRadioDisabledBudgeted(radio_, kBleConnTxDisableWaitUs,
                                     spinLimit / 2U + 1U);
  }
  if (!txOk) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitRadioDisabledBudgeted(radio_, kBleConnTxDisableWaitUs,
                              spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    return false;
  }
  if (txBuiltFreshEncrypted) {
    connectionEncTxCounter_ =
        (txEncryptedCounter + 1ULL) & kBleEncPacketCounterMask;
  }
  if ((txLlid == kBlePduLlControl) &&
      (connectionLastTxPlainLength_ >= 1U)) {
    const uint8_t opcode = connectionLastTxPlainPayload_[0];
    if (opcode == kBleLlCtrlEncReq) {
      emitBleTrace("LL_ENC_REQ_TX");
    } else if (opcode == kBleLlCtrlStartEncReq) {
      connectionEncStartReqTxPending_ = false;
      connectionEncAwaitingStartRsp_ = true;
      connectionEncRxEnabled_ = true;
      connectionEncEnableTxOnNextEvent_ = false;
      emitBleTrace("LL_START_ENC_REQ_TX");
    }
  }
  connectionTxHistoryValid_ = true;

  // With DISABLED->RXEN armed for the central turnaround, a fast peer response
  // can complete before we finish TX cleanup. Preserve that real RX completion
  // instead of clearing it as stale TX state.
  const uint32_t postTxState =
      (radio_->STATE & RADIO_STATE_STATE_Msk) >> RADIO_STATE_STATE_Pos;
  const bool followRxAlreadyDone =
      (radio_->EVENTS_CRCOK != 0U) || (radio_->EVENTS_CRCERROR != 0U);
  {
    uint8_t observeFlags = 0U;
    if (radio_->EVENTS_RXREADY != 0U) {
      observeFlags |= 0x01U;
    }
    if (radio_->EVENTS_ADDRESS != 0U) {
      observeFlags |= 0x02U;
    }
    if (radio_->EVENTS_CRCOK != 0U) {
      observeFlags |= 0x04U;
    }
    if (radio_->EVENTS_CRCERROR != 0U) {
      observeFlags |= 0x08U;
    }
    if (followRxAlreadyDone) {
      observeFlags |= 0x10U;
    }
    g_ble_central_posttx_observe_flags = observeFlags;
    g_ble_central_posttx_observe_state = static_cast<uint8_t>(postTxState);
    ++g_ble_central_posttx_observe_count;
  }
  if (followRxAlreadyDone) {
    ++g_ble_central_follow_rx_preserved_count;
  }

  // Clear TX completion state before waiting on follow-up RX; otherwise the
  // stale TX END/PHYEND can be mistaken for a received packet.
  radio_->EVENTS_END = 0U;
  radio_->EVENTS_PHYEND = 0U;
  radio_->EVENTS_DISABLED = 0U;
  if (!followRxAlreadyDone) {
    radio_->EVENTS_CRCOK = 0U;
    radio_->EVENTS_CRCERROR = 0U;
  }
  radio_->SHORTS =
      ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
       RADIO_SHORTS_RXREADY_START_Msk) |
      ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
        << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
       RADIO_SHORTS_ADDRESS_RSSISTART_Msk) |
      ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
       RADIO_SHORTS_PHYEND_DISABLE_Msk);

  if (!followRxAlreadyDone) {
    const uint32_t state =
        (radio_->STATE & RADIO_STATE_STATE_Msk) >> RADIO_STATE_STATE_Pos;
    if (state == RADIO_STATE_STATE_Disabled) {
      radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;
    }
  }

  uint32_t followListenUs = kBleConnFollowupRxListenMaxUs;
  const uint16_t rxPayloadLength = currentRxDataPduPayloadLength();
  const uint32_t dynamicFollowListenUs =
      bleDataPduAirTimeUsOnPhy(static_cast<uint8_t>(rxPayloadLength),
                               connectionCurrentRxPhy_) +
      1200U;
  if (followListenUs < dynamicFollowListenUs) {
    followListenUs = dynamicFollowListenUs;
  }
  const uint32_t intervalUs =
      (connectionIntervalUnits_ > 0U)
          ? (static_cast<uint32_t>(connectionIntervalUnits_) * 1250UL)
          : 1250UL;
  if (((connectionCurrentRxPhy_ & kBlePhyCoded) == 0U) &&
      intervalUs > 3000U) {
    const uint32_t capUs = intervalUs - 3000U;
    if (followListenUs > capUs) {
      followListenUs = capUs;
    }
  }

  const bool rxDoneSeen =
      followRxAlreadyDone ||
      waitRadioRxDoneBudgeted(radio_, followListenUs, spinLimit / 2U + 1U);
  if (!rxDoneSeen) {
    uint8_t timeoutFlags = 0U;
    if (radio_->EVENTS_RXREADY != 0U) {
      timeoutFlags |= 0x01U;
    }
    if (radio_->EVENTS_ADDRESS != 0U) {
      timeoutFlags |= 0x02U;
    }
    if (radio_->EVENTS_CRCOK != 0U) {
      timeoutFlags |= 0x04U;
    }
    if (radio_->EVENTS_CRCERROR != 0U) {
      timeoutFlags |= 0x08U;
    }
    g_ble_central_rxtimeout_observe_flags = timeoutFlags;
    g_ble_central_rxtimeout_observe_state = static_cast<uint8_t>(
        (radio_->STATE & RADIO_STATE_STATE_Msk) >> RADIO_STATE_STATE_Pos);
    g_ble_central_rxtimeout_hdr0 = rxPacket_[0];
    g_ble_central_rxtimeout_hdr1 = rxPacket_[1];
    g_ble_central_rxtimeout_hdr2 = rxPacket_[2];
    g_ble_central_rxtimeout_hdr3 = rxPacket_[3];
    ++g_ble_central_rxtimeout_observe_count;
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs, spinLimit / 2U + 1U);
    radio_->SHORTS = 0U;
    clearRadioCoreEvents(radio_);
    connectionFreshTxAllowed_ = false;
    ++connectionMissedEventCount_;

    const uint32_t intervalUs =
        (connectionIntervalUnits_ > 0U)
            ? (static_cast<uint32_t>(connectionIntervalUnits_) * 1250UL)
            : 1250UL;
    const uint32_t timeoutUs =
        static_cast<uint32_t>(connectionTimeoutUnits_) * 10000UL;
    uint32_t supervisionEvents = timeoutUs / intervalUs;
    if (supervisionEvents < 1U) {
      supervisionEvents = 1U;
    }
    if (connectionMissedEventCount_ >= supervisionEvents) {
      if (event != nullptr) {
        event->terminateInd = true;
      }
      emitBleTrace("SUPERVISION_TIMEOUT");
      disconnectWithReason(BleDisconnectReason::kSupervisionTimeout, 0U,
                           spinLimit / 2U + 1U);
    }

    if (event != nullptr) {
      event->txPacketSent = true;
      event->txLlid = txLlid;
      event->txNesn = txNesnBit;
      event->txSn = txSnBit;
      event->txPayloadLength = txLength;
      event->txPayload =
          (txLength > 0U) ? &connectionLastTxPlainPayload_[0] : nullptr;
      event->emptyAckTransmitted = (txLlid == 0x01U) && (txLength == 0U);
      event->freshTxAllowed = canSendNewPayloadThisEvent;
    }
    emitBleTrace("CENTRAL_EVT_RX_TIMEOUT");
    return true;
  }

  bool rxDisabled = waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                              spinLimit / 2U + 1U);
  if (!rxDisabled) {
    radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
    rxDisabled = waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                           spinLimit / 2U + 1U);
  }
  const uint32_t crcStatus =
      (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
      RADIO_CRCSTATUS_CRCSTATUS_Pos;
  const bool crcOk = (crcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
  radio_->SHORTS = 0U;
  if (!rxDisabled) {
    clearRadioCoreEvents(radio_);
    return false;
  }

  if (event != nullptr) {
    event->packetReceived = true;
    event->crcOk = crcOk;
    event->rssiDbm = radioRssiDbm(radio_);
    event->payload = &rxPacket_[2];
  }

  if (!crcOk) {
    connectionFreshTxAllowed_ = false;
    ++connectionMissedEventCount_;
    clearRadioCoreEvents(radio_);
    return true;
  }
  connectionMissedEventCount_ = 0U;

  const uint8_t hdr0 = rxPacket_[0];
  const uint8_t llid = hdr0 & 0x03U;
  const uint8_t nesn = (hdr0 >> 2U) & 0x01U;
  const uint8_t sn = (hdr0 >> 3U) & 0x01U;
  uint8_t rxLength = rxPacket_[1];
  const uint8_t rxLengthRaw = rxLength;

  bool peerAckedLastTx =
      connectionTxHistoryValid_ && (nesn != connectionTxSn_);
  const bool packetIsNew = (sn == connectionExpectedRxSn_);
  if (packetIsNew) {
    connectionExpectedRxSn_ ^= 0x01U;
  }
  bool terminateInd = false;
  bool peerTerminateIndReceived = false;
  uint8_t peerTerminateErrorCode = 0U;
  if (packetIsNew &&
      connectionEncSessionValid_ &&
      connectionEncAwaitingStartRsp_ &&
      (llid == kBlePduLlControl) &&
      (rxLength >= 1U)) {
    if ((rxLength == 1U) &&
        (rxPacket_[2] == kBleLlCtrlStartEncRsp)) {
      emitBleTrace("LL_START_ENC_RSP_PLAIN");
    } else if (rxLength == static_cast<uint8_t>(1U + kBleMicLen)) {
      uint8_t plaintext[8] = {0};
      uint8_t plaintextLen = 0U;
      auto tryDecryptStartEncRsp = [&](const uint8_t key[16], uint8_t rxDirection,
                                       bool adoptAltKey, bool swapDirection) {
        if (!bleCcmDecryptPayload(key, connectionEncIv_, connectionEncRxCounter_,
                                  rxDirection, hdr0, &rxPacket_[2], rxLength,
                                  plaintext, &plaintextLen)) {
          return false;
        }
        if (plaintextLen != 1U ||
            plaintext[0] != kBleLlCtrlStartEncRsp) {
          return false;
        }
        if (adoptAltKey) {
          memcpy(connectionEncSessionKey_, connectionEncSessionKeyAlt_,
                 sizeof(connectionEncSessionKey_));
          connectionEncAltKeyValid_ = false;
          memcpy(encDebug_.encLastSessionKey, connectionEncSessionKey_,
                 sizeof(encDebug_.encLastSessionKey));
          encDebug_.encLastSessionKeyValid = 1U;
        }
        if (swapDirection) {
          connectionEncRxDirection_ = rxDirection;
          connectionEncTxDirection_ = static_cast<uint8_t>(rxDirection ^ 0x01U);
          encDebug_.encLastRxDir = connectionEncRxDirection_;
          encDebug_.encLastTxDir = connectionEncTxDirection_;
        }
        rxPacket_[2] = kBleLlCtrlStartEncRsp;
        rxLength = 1U;
        connectionEncRxCounter_ =
            (connectionEncRxCounter_ + 1ULL) & kBleEncPacketCounterMask;
        emitBleTrace(adoptAltKey
                         ? (swapDirection ? "ENC_KEY_DIR_SWAP"
                                          : "ENC_KEY_SWAP")
                         : (swapDirection ? "ENC_DIR_SWAP"
                                          : "LL_START_ENC_RSP_DEC"));
        return true;
      };

      const uint8_t swappedRxDirection =
          static_cast<uint8_t>(connectionEncRxDirection_ ^ 0x01U);
      const bool decrypted =
          tryDecryptStartEncRsp(connectionEncSessionKey_,
                                connectionEncRxDirection_, false, false) ||
          tryDecryptStartEncRsp(connectionEncSessionKey_, swappedRxDirection,
                                false, true) ||
          (connectionEncAltKeyValid_ &&
           tryDecryptStartEncRsp(connectionEncSessionKeyAlt_,
                                 connectionEncRxDirection_, true, false)) ||
          (connectionEncAltKeyValid_ &&
           tryDecryptStartEncRsp(connectionEncSessionKeyAlt_, swappedRxDirection,
                                 true, true));
      if (!decrypted) {
        emitBleTrace("LL_START_ENC_RSP_DEC_FAIL");
      }
    }
  }
  if (connectionEncSessionValid_ &&
      connectionEncRxEnabled_ &&
      !connectionEncAwaitingStartRsp_) {
    if (rxLengthRaw < kBleMicLen) {
      const bool allowPostStartPlainZeroLen =
          (rxLengthRaw == 0U) &&
          (connectionEncRxCounter_ <= 3ULL) &&
          ((llid == kBlePduDataStartOrComplete) || (llid == 0x01U));
      if (!allowPostStartPlainZeroLen) {
        terminateInd = true;
        ++encDebug_.encRxShortPduCount;
        emitBleTrace("ENC_RX_SHORT_PDU");
      }
    } else {
      uint8_t plaintext[kBleDataPduMaxPayload] = {0};
      uint8_t plaintextLen = 0U;
      uint64_t rxCounter = connectionEncRxCounter_;
      if (!packetIsNew && (rxCounter > 0ULL)) {
        rxCounter =
            (rxCounter - 1ULL) & kBleEncPacketCounterMask;
      }
      auto tryDecryptEncryptedPdu = [&](const uint8_t key[16], uint8_t rxDirection,
                                        bool adoptAltKey, bool swapDirection) {
        if (!bleCcmDecryptPayload(key, connectionEncIv_, rxCounter,
                                  rxDirection, hdr0, &rxPacket_[2], rxLengthRaw,
                                  plaintext, &plaintextLen)) {
          return false;
        }
        if (adoptAltKey) {
          memcpy(connectionEncSessionKey_, connectionEncSessionKeyAlt_,
                 sizeof(connectionEncSessionKey_));
          connectionEncAltKeyValid_ = false;
          memcpy(encDebug_.encLastSessionKey, connectionEncSessionKey_,
                 sizeof(encDebug_.encLastSessionKey));
          encDebug_.encLastSessionKeyValid = 1U;
        }
        if (swapDirection) {
          connectionEncRxDirection_ = rxDirection;
          connectionEncTxDirection_ = static_cast<uint8_t>(rxDirection ^ 0x01U);
          encDebug_.encLastRxDir = connectionEncRxDirection_;
          encDebug_.encLastTxDir = connectionEncTxDirection_;
        }
        rxLength = plaintextLen;
        if (plaintextLen > 0U) {
          memcpy(&rxPacket_[2], plaintext, plaintextLen);
        }
        if (packetIsNew) {
          connectionEncRxCounter_ =
              (connectionEncRxCounter_ + 1ULL) & kBleEncPacketCounterMask;
        }
        return true;
      };

      const uint8_t swappedRxDirection =
          static_cast<uint8_t>(connectionEncRxDirection_ ^ 0x01U);
      const bool decrypted =
          tryDecryptEncryptedPdu(connectionEncSessionKey_,
                                 connectionEncRxDirection_, false, false) ||
          tryDecryptEncryptedPdu(connectionEncSessionKey_, swappedRxDirection,
                                 false, true) ||
          (connectionEncAltKeyValid_ &&
           tryDecryptEncryptedPdu(connectionEncSessionKeyAlt_,
                                  connectionEncRxDirection_, true, false)) ||
          (connectionEncAltKeyValid_ &&
           tryDecryptEncryptedPdu(connectionEncSessionKeyAlt_, swappedRxDirection,
                                  true, true));
      if (!decrypted) {
        terminateInd = true;
        ++encDebug_.encRxMicFailCount;
        encDebug_.encRxLastMicFailHdr = hdr0;
        encDebug_.encRxLastMicFailLenRaw = rxLengthRaw;
        encDebug_.encRxLastMicFailDir = connectionEncRxDirection_;
        encDebug_.encRxLastMicFailCounterLo =
            static_cast<uint32_t>(connectionEncRxCounter_ & 0xFFFFFFFFULL);
        emitBleTrace("ENC_RX_MIC_FAIL");
      }
    }
  }
  bool implicitEmptyAck = false;
  const bool lastTxWasEmptyAck =
      connectionTxHistoryValid_ &&
      (connectionLastTxLlid_ == 0x01U) &&
      (connectionLastTxLength_ == 0U);
  const bool lastTxHadAttPayload =
      connectionTxHistoryValid_ &&
      (connectionLastTxLlid_ == kBlePduDataStartOrComplete) &&
      (connectionLastTxPlainLength_ >= (kBleL2capHeaderLen + 1U)) &&
      (readLe16(&connectionLastTxPlainPayload_[2]) == kBleL2capCidAtt);
  const bool peerAdvancedToNewAttServerProgress =
      packetIsNew &&
      (llid == kBlePduDataStartOrComplete) &&
      (rxLength >= (kBleL2capHeaderLen + 1U)) &&
      (readLe16(&rxPacket_[4]) == kBleL2capCidAtt) &&
      (attOpcodeIsServerResponsePdu(rxPacket_[2 + kBleL2capHeaderLen]) ||
       (rxPacket_[2 + kBleL2capHeaderLen] == kAttOpHandleValueNtf) ||
       (rxPacket_[2 + kBleL2capHeaderLen] == kAttOpHandleValueInd));
  // Some peers keep NESN stale while they continue pushing notifications or a
  // server response. Treat that higher-layer progress as an implicit ACK so
  // the central can advance SN and deliver the next queued ATT request, both
  // before and after link-layer encryption comes up.
  if (!peerAckedLastTx &&
      connectionTxHistoryValid_ &&
      packetIsNew &&
      ((lastTxWasEmptyAck && peerAdvancedToNewAttServerProgress) ||
       (lastTxHadAttPayload && peerAdvancedToNewAttServerProgress))) {
    peerAckedLastTx = true;
    implicitEmptyAck = true;
    if (lastTxHadAttPayload && peerAdvancedToNewAttServerProgress) {
      ++encDebug_.connImplicitAttProgressAckCount;
    }
  }
  if (peerAckedLastTx) {
    connectionTxSn_ ^= 0x01U;
    connectionFreshTxAllowed_ = true;
  } else {
    connectionFreshTxAllowed_ = false;
  }
  if ((llid == kBlePduLlControl) &&
      (rxLength >= 2U) &&
      (rxPacket_[2] == kBleLlCtrlTerminateInd)) {
    terminateInd = true;
    peerTerminateIndReceived = true;
    peerTerminateErrorCode = rxPacket_[3];
  }

  rememberConnectionRxDebug(
      llid, rxLength,
      bleConnectionPayloadProtocolOpcode(llid, &rxPacket_[2], rxLength), nesn, sn,
      packetIsNew, peerAckedLastTx);

  if (event != nullptr) {
    event->packetIsNew = packetIsNew;
    event->peerAckedLastTx = peerAckedLastTx;
    event->freshTxAllowed = (!connectionTxHistoryValid_ || peerAckedLastTx);
    event->implicitEmptyAck = implicitEmptyAck;
    event->llid = llid;
    event->rxNesn = nesn;
    event->rxSn = sn;
    event->payloadLength = rxLength;
    if (packetIsNew && llid == kBlePduLlControl && rxLength >= 1U) {
      event->llControlPacket = true;
      event->llControlOpcode = rxPacket_[2];
    }
    if (packetIsNew &&
        llid == kBlePduDataStartOrComplete &&
        rxLength >= 5U) {
      const uint16_t l2capLen = readLe16(&rxPacket_[2]);
      const uint16_t cid = readLe16(&rxPacket_[4]);
      const uint8_t availableAttLen = (rxLength > kBleL2capHeaderLen)
                                          ? static_cast<uint8_t>(rxLength - kBleL2capHeaderLen)
                                          : 0U;
      if (cid == kBleL2capCidAtt && l2capLen > 0U && availableAttLen > 0U) {
        event->attPacket = true;
        event->attOpcode = rxPacket_[2 + kBleL2capHeaderLen];
      }
    }
  }

  const bool rxHasL2cap =
      packetIsNew &&
      (llid == kBlePduDataStartOrComplete) &&
      (rxLength >= kBleL2capHeaderLen);
  const uint16_t rxL2capLen = rxHasL2cap ? readLe16(&rxPacket_[2]) : 0U;
  const uint16_t rxL2capCid = rxHasL2cap ? readLe16(&rxPacket_[4]) : 0U;
  const uint8_t rxAttOpcode =
      (rxHasL2cap && rxLength > kBleL2capHeaderLen)
          ? rxPacket_[2 + kBleL2capHeaderLen]
          : 0U;

  if (packetIsNew &&
      (llid == kBlePduLlControl) &&
      (rxLength >= 1U)) {
    const uint8_t opcode = rxPacket_[2];
    uint8_t deferredLlControlResponse[kBleDataPduMaxPayload];
    if (opcode == kBleLlCtrlRejectInd && rxLength >= 2U &&
        rxPacket_[3] == kBleLlCtrlLengthReq) {
      connectionDataLengthUpdatePending_ = false;
      connectionDataLengthUpdateInFlight_ = false;
    } else if (opcode == kBleLlCtrlRejectExtInd && rxLength >= 3U &&
               rxPacket_[3] == kBleLlCtrlLengthReq) {
      connectionDataLengthUpdatePending_ = false;
      connectionDataLengthUpdateInFlight_ = false;
    }

    if (!terminateInd) {
      uint8_t responseLength = 0U;
      bool responseTerminate = false;
      uint8_t* responseBuffer =
          connectionPendingTxValid_ ? deferredLlControlResponse
                                    : connectionPendingTxPayload_;
      if (buildLlControlResponse(&rxPacket_[2], rxLength, currentEventCounter,
                                 responseBuffer, &responseLength,
                                 &responseTerminate) &&
          responseLength > 0U &&
          !connectionPendingTxValid_) {
        connectionPendingTxLlid_ = kBlePduLlControl;
        connectionPendingTxLength_ = responseLength;
        connectionPendingTxValid_ = true;
      }
      if (responseTerminate) {
        terminateInd = true;
      }
    }
  }

  if (rxHasL2cap && (rxL2capCid == kBleL2capCidAtt) &&
      (rxL2capLen >= 3U) &&
      (rxAttOpcode == kAttOpExchangeMtuRsp)) {
    const uint16_t serverRxMtu =
        readLe16(&rxPacket_[2 + kBleL2capHeaderLen + 1U]);
    uint16_t localRxMtu = connectionRequestedAttMtu_;
    if (localRxMtu < kBleDefaultAttMtu) {
      localRxMtu = kBleDefaultAttMtu;
    } else if (localRxMtu > localPreferredAttMtu()) {
      localRxMtu = localPreferredAttMtu();
    }
    const uint16_t negotiatedMtu = minU16(serverRxMtu, localRxMtu);
    connectionAttMtu_ = (negotiatedMtu >= kBleDefaultAttMtu)
                            ? negotiatedMtu
                            : kBleDefaultAttMtu;
    connectionAttMtuExchangePending_ = false;
    connectionAttMtuExchangeInFlight_ = false;
    connectionAttMtuExchangeComplete_ = true;
    emitBleTrace("ATT_MTU_RSP_RX");
  }

  if (rxHasL2cap && !terminateInd &&
      !connectionPendingTxValid_) {
    uint8_t responseLength = 0U;
    if (buildL2capResponse(llid, &rxPacket_[2], rxLength, packetIsNew,
                           connectionPendingTxPayload_, &responseLength) &&
        responseLength > 0U) {
      connectionPendingTxLlid_ = kBlePduDataStartOrComplete;
      connectionPendingTxLength_ = responseLength;
      connectionPendingTxValid_ = true;
    }
  }

  if (rxHasL2cap &&
      (rxL2capCid == kBleL2capCidAtt) &&
      (rxL2capLen > 0U) &&
      (rxAttOpcode == kAttOpHandleValueInd) &&
      !connectionPendingTxValid_) {
      writeLe16(&connectionPendingTxPayload_[0], 1U);
      writeLe16(&connectionPendingTxPayload_[2], kBleL2capCidAtt);
      connectionPendingTxPayload_[4] = kAttOpHandleValueCfm;
      connectionPendingTxLlid_ = kBlePduDataStartOrComplete;
      connectionPendingTxLength_ = 5U;
      connectionPendingTxValid_ = true;
  }

  maybeQueueCentralLinkSetupRequest();

  auto pendingTxIsHandleValueCfm = [&]() {
    return connectionPendingTxValid_ &&
           (connectionPendingTxLlid_ == kBlePduDataStartOrComplete) &&
           (connectionPendingTxLength_ == 5U) &&
           (readLe16(&connectionPendingTxPayload_[2]) == kBleL2capCidAtt) &&
           (connectionPendingTxPayload_[4] == kAttOpHandleValueCfm);
  };
  const bool currentRxPeerMoreData = (((hdr0 >> 4U) & 0x01U) != 0U);
  const bool currentRxIsAttServerResponse =
      rxHasL2cap &&
      (rxL2capCid == kBleL2capCidAtt) &&
      attOpcodeIsServerResponsePdu(rxAttOpcode);
  const bool currentRxIsAttServerPush =
      rxHasL2cap &&
      (rxL2capCid == kBleL2capCidAtt) &&
      ((rxAttOpcode == kAttOpHandleValueNtf) ||
       (rxAttOpcode == kAttOpHandleValueInd));
  const bool currentRxIsAttServerIndication =
      rxHasL2cap &&
      (rxL2capCid == kBleL2capCidAtt) &&
      (rxAttOpcode == kAttOpHandleValueInd);
  const bool continueCentralTurnaround =
      !terminateInd &&
      connectionEncSessionValid_ &&
      connectionEncTxEnabled_ &&
      (currentRxIsAttServerPush ||
       (currentRxIsAttServerResponse && currentRxPeerMoreData));
  uint8_t originalRxPayload[kBleDataPduMaxPayload] = {0};
  const bool restoreOriginalPacketAfterTurnaround =
      (rxLength > 0U) && (rxLength <= kBleDataPduMaxPayload);
  if (restoreOriginalPacketAfterTurnaround) {
    memcpy(originalRxPayload, &rxPacket_[2], rxLength);
  }

  if (continueCentralTurnaround) {
    uint32_t followListenUs = kBleConnFollowupRxListenMaxUs;
    const uint16_t rxPayloadLength = currentRxDataPduPayloadLength();
    const uint16_t txPayloadLength = currentTxDataPduPayloadLength();
    const uint32_t dynamicFollowListenUs =
        bleDataPduAirTimeUsOnPhy(static_cast<uint8_t>(rxPayloadLength),
                                 connectionCurrentRxPhy_) +
        bleConnectionTxEndBudgetUsOnPhy(static_cast<uint8_t>(txPayloadLength),
                                        connectionCurrentTxPhy_) +
        1200U;
    if (followListenUs < dynamicFollowListenUs) {
      followListenUs = dynamicFollowListenUs;
    }
    const uint32_t followIntervalUs =
        (connectionIntervalUnits_ > 0U)
            ? (static_cast<uint32_t>(connectionIntervalUnits_) * 1250UL)
            : 1250UL;
    if (((connectionCurrentRxPhy_ & kBlePhyCoded) == 0U) &&
        followIntervalUs > 3000U) {
      const uint32_t capUs = followIntervalUs - 3000U;
      if (followListenUs > capUs) {
        followListenUs = capUs;
      }
    }

    const uint32_t followStartUs = bleTimingUs();
    uint32_t followRxEndTimestampUs = bleTimingUs();
    uint8_t followPackets = 0U;
    bool keepTurningAround = true;
    bool currentFollowupPeerMoreData = (((hdr0 >> 4U) & 0x01U) != 0U);
    bool followMustAckPeerServerPush = currentRxIsAttServerIndication;
    while (!terminateInd && keepTurningAround && (followPackets < 8U)) {
      const uint32_t followNowUs = bleTimingUs();
      const uint32_t elapsedUs =
          static_cast<uint32_t>(followNowUs - followStartUs);
      if (elapsedUs >= followListenUs) {
        break;
      }

      uint8_t txFollowLlid = 0x01U;
      uint8_t txFollowLength = 0U;
      if (pendingTxIsHandleValueCfm()) {
        txFollowLlid = connectionPendingTxLlid_;
        txFollowLength = connectionPendingTxLength_;
        if (txFollowLength > 0U) {
          memcpy(connectionTxPayload_, connectionPendingTxPayload_, txFollowLength);
        }
        connectionPendingTxLlid_ = 0x01U;
        connectionPendingTxLength_ = 0U;
        connectionPendingTxValid_ = false;
      } else if (!followMustAckPeerServerPush &&
                 !currentFollowupPeerMoreData &&
                 connectionFreshTxAllowed_ &&
                 connectionPendingTxValid_) {
        txFollowLlid = connectionPendingTxLlid_;
        txFollowLength = connectionPendingTxLength_;
        if (txFollowLength > 0U) {
          memcpy(connectionTxPayload_, connectionPendingTxPayload_, txFollowLength);
        }
        connectionPendingTxLlid_ = 0x01U;
        connectionPendingTxLength_ = 0U;
        connectionPendingTxValid_ = false;
      }

      connectionLastTxLlid_ = txFollowLlid;
      connectionLastTxLength_ = txFollowLength;
      connectionLastTxPlainLlid_ = txFollowLlid;
      connectionLastTxPlainLength_ = txFollowLength;
      memset(connectionLastTxPlainPayload_, 0, sizeof(connectionLastTxPlainPayload_));
      if (txFollowLength > 0U) {
        memcpy(connectionLastTxPlainPayload_, connectionTxPayload_, txFollowLength);
      }

      const bool txFollowHasFollowingPayload =
          (txFollowLlid == 0x01U) &&
          (txFollowLength == 0U) &&
          (connectionPendingTxValid_ ||
           (connectionCustomNotificationQueueCount_ > 0U));
      txPacket_[0] = static_cast<uint8_t>((txFollowLlid & 0x03U) |
                                          ((connectionExpectedRxSn_ & 0x01U) << 2U) |
                                          ((connectionTxSn_ & 0x01U) << 3U) |
                                          (txFollowHasFollowingPayload ? 0x10U : 0U));
      bool encryptFollowTx = false;
      uint64_t followTxCounter = connectionEncTxCounter_;
      uint8_t txFollowLengthOnAir = txFollowLength;
      const bool txFollowIsEmptyAck =
          (txFollowLlid == 0x01U) &&
          (txFollowLength == 0U);
      if (connectionEncSessionValid_ &&
          connectionEncTxEnabled_) {
        uint8_t encryptedLength = 0U;
        if (txFollowIsEmptyAck &&
            connectionEncPrecomputedEmptyValid_ &&
            (connectionEncPrecomputedCounter_ == followTxCounter)) {
          encryptedLength = kBleMicLen;
          memcpy(connectionLastTxEncryptedPayload_, connectionEncPrecomputedPayload_,
                 kBleMicLen);
          encryptFollowTx = true;
        } else {
          encryptFollowTx =
              bleCcmEncryptPayload(connectionEncSessionKey_, connectionEncIv_,
                                   followTxCounter, connectionEncTxDirection_,
                                   txPacket_[0],
                                   (txFollowLength > 0U) ? connectionTxPayload_
                                                         : nullptr,
                                   txFollowLength,
                                   connectionLastTxEncryptedPayload_,
                                   &encryptedLength);
        }
        if (!encryptFollowTx) {
          terminateInd = true;
          break;
        }
        connectionLastTxWasEncrypted_ = true;
        connectionLastTxEncryptedLength_ = encryptedLength;
        txFollowLengthOnAir = encryptedLength;
        txPacket_[1] = encryptedLength;
        if (encryptedLength > 0U) {
          memcpy(&txPacket_[2], connectionLastTxEncryptedPayload_, encryptedLength);
        }
      } else {
        connectionLastTxWasEncrypted_ = false;
        connectionLastTxEncryptedLength_ = 0U;
        txPacket_[1] = txFollowLength;
        if (txFollowLength > 0U) {
          memcpy(&txPacket_[2], connectionTxPayload_, txFollowLength);
        }
      }

      clearRadioCoreEvents(radio_);
      {
        const size_t txCopyLen = 2U + static_cast<size_t>(txFollowLengthOnAir);
        const size_t copyLen =
            (txCopyLen <= sizeof(rxPacket_)) ? txCopyLen : sizeof(rxPacket_);
        memcpy(rxPacket_, txPacket_, copyLen);
      }
      radio_->PACKETPTR =
          static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&rxPacket_[0]));
      radio_->SHORTS =
          ((RADIO_SHORTS_TXREADY_START_Enabled << RADIO_SHORTS_TXREADY_START_Pos) &
           RADIO_SHORTS_TXREADY_START_Msk) |
          ((RADIO_SHORTS_PHYEND_DISABLE_Enabled << RADIO_SHORTS_PHYEND_DISABLE_Pos) &
           RADIO_SHORTS_PHYEND_DISABLE_Msk) |
          ((RADIO_SHORTS_DISABLED_RXEN_Enabled << RADIO_SHORTS_DISABLED_RXEN_Pos) &
           RADIO_SHORTS_DISABLED_RXEN_Msk) |
          ((RADIO_SHORTS_RXREADY_START_Enabled << RADIO_SHORTS_RXREADY_START_Pos) &
           RADIO_SHORTS_RXREADY_START_Msk) |
          ((RADIO_SHORTS_ADDRESS_RSSISTART_Enabled
            << RADIO_SHORTS_ADDRESS_RSSISTART_Pos) &
           RADIO_SHORTS_ADDRESS_RSSISTART_Msk);

      const uint32_t txFollowTriggerTargetUs =
          followRxEndTimestampUs + kBleConnTxenAfterRxUs;
      bleTriggerTxAtTargetUsCritical(radio_, txFollowTriggerTargetUs);

      const uint32_t txFollowEndBudgetUs =
          bleConnectionTxEndBudgetUsOnPhy(txFollowLengthOnAir,
                                          connectionCurrentTxPhy_);
      const bool txFollowEndSeen =
          waitRadioEndBudgeted(radio_, txFollowEndBudgetUs, spinLimit / 2U + 1U);
      bool txFollowOk = false;
      if (txFollowEndSeen) {
        radio_->EVENTS_END = 0U;
        radio_->EVENTS_PHYEND = 0U;
        txFollowOk =
            waitRadioDisabledBudgeted(radio_, kBleConnTxDisableWaitUs,
                                      spinLimit / 2U + 1U);
      }
      if (!txFollowOk) {
        radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
        waitRadioDisabledBudgeted(radio_, kBleConnTxDisableWaitUs,
                                  spinLimit / 2U + 1U);
        radio_->SHORTS = 0U;
        break;
      }
      const bool followRxAlreadyDone =
          (radio_->EVENTS_CRCOK != 0U) || (radio_->EVENTS_CRCERROR != 0U);
      if (encryptFollowTx) {
        connectionEncTxCounter_ =
            (followTxCounter + 1ULL) & kBleEncPacketCounterMask;
      }
      connectionTxHistoryValid_ = true;
      const bool txFollowCarriesAttRequest =
          (txFollowLlid == kBlePduDataStartOrComplete) &&
          (txFollowLength >= (kBleL2capHeaderLen + 1U)) &&
          (readLe16(&connectionLastTxPlainPayload_[2]) == kBleL2capCidAtt);
      const bool waitForPostAckResponse =
          currentFollowupPeerMoreData ||
          txFollowHasFollowingPayload ||
          txFollowCarriesAttRequest;

      radio_->EVENTS_DISABLED = 0U;
      if (!followRxAlreadyDone) {
        radio_->EVENTS_CRCOK = 0U;
        radio_->EVENTS_CRCERROR = 0U;
      }
      radio_->SHORTS = bleRadioShortsRxListen();
      if (!followRxAlreadyDone) {
        const uint32_t state =
            (radio_->STATE & RADIO_STATE_STATE_Msk) >> RADIO_STATE_STATE_Pos;
        if (state == RADIO_STATE_STATE_Disabled) {
          radio_->TASKS_RXEN = RADIO_TASKS_RXEN_TASKS_RXEN_Trigger;
        }
      }
      if (!waitForPostAckResponse && !followRxAlreadyDone) {
        radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
        waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                  spinLimit / 2U + 1U);
        radio_->SHORTS = 0U;
        // We just transmitted a fresh follow-up PDU and intentionally ended
        // the event before receiving the peer's next packet, so that new TX
        // has not been ACKed yet. Force the next event to reuse this exact
        // ciphertext instead of consuming a new CCM counter on another fresh
        // empty/data PDU.
        connectionFreshTxAllowed_ = false;
        break;
      }

      const uint32_t followNowUsAfterTx = bleTimingUs();
      const uint32_t elapsedUsAfterTx =
          static_cast<uint32_t>(followNowUsAfterTx - followStartUs);
      if (elapsedUsAfterTx >= followListenUs) {
        radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
        waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                  spinLimit / 2U + 1U);
        radio_->SHORTS = 0U;
        connectionFreshTxAllowed_ = false;
        break;
      }
      const uint32_t remainingUs = followListenUs - elapsedUsAfterTx;
      const bool followRxDone =
          followRxAlreadyDone ||
          waitRadioRxDoneBudgeted(radio_, remainingUs, spinLimit / 2U + 1U,
                                  &followRxEndTimestampUs);
      if (!followRxDone) {
        radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
        waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                  spinLimit / 2U + 1U);
        radio_->SHORTS = 0U;
        connectionFreshTxAllowed_ = false;
        break;
      }

      bool followDisabled =
          waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                    spinLimit / 2U + 1U);
      if (!followDisabled) {
        radio_->TASKS_DISABLE = RADIO_TASKS_DISABLE_TASKS_DISABLE_Trigger;
        followDisabled =
            waitRadioDisabledBudgeted(radio_, kBleConnDisableWaitUs,
                                      spinLimit / 2U + 1U);
      }
      radio_->SHORTS = 0U;
      if (!followDisabled) {
        break;
      }

      ++followPackets;
      const uint32_t followCrcStatus =
          (radio_->CRCSTATUS & RADIO_CRCSTATUS_CRCSTATUS_Msk) >>
          RADIO_CRCSTATUS_CRCSTATUS_Pos;
      const bool followCrcOk =
          (followCrcStatus == RADIO_CRCSTATUS_CRCSTATUS_CRCOk);
      if (!followCrcOk) {
        connectionFreshTxAllowed_ = false;
        break;
      }

      const uint8_t hdrFollow = rxPacket_[0];
      const uint8_t llidFollow = hdrFollow & 0x03U;
      const uint8_t nesnFollow = (hdrFollow >> 2U) & 0x01U;
      const uint8_t snFollow = (hdrFollow >> 3U) & 0x01U;
      uint8_t rxLengthFollow = rxPacket_[1];
      const uint8_t rxLengthFollowRaw = rxLengthFollow;

      bool peerAckedLastTxFollow =
          connectionTxHistoryValid_ && (nesnFollow != connectionTxSn_);
      const bool packetIsNewFollow = (snFollow == connectionExpectedRxSn_);
      if (packetIsNewFollow) {
        connectionExpectedRxSn_ ^= 0x01U;
      }

      if (connectionEncSessionValid_ &&
          connectionEncRxEnabled_) {
        if (rxLengthFollowRaw < kBleMicLen) {
          terminateInd = true;
          break;
        }

        uint8_t plaintext[kBleDataPduMaxPayload] = {0};
        uint8_t plaintextLen = 0U;
        uint64_t rxCounter = connectionEncRxCounter_;
        if (!packetIsNewFollow && (rxCounter > 0ULL)) {
          rxCounter =
              (rxCounter - 1ULL) & kBleEncPacketCounterMask;
        }
        auto tryDecryptFollowEncryptedPdu =
            [&](const uint8_t key[16], uint8_t rxDirection,
                bool adoptAltKey, bool swapDirection) {
              if (!bleCcmDecryptPayload(key, connectionEncIv_,
                                        rxCounter, rxDirection,
                                        hdrFollow, &rxPacket_[2],
                                        rxLengthFollowRaw, plaintext,
                                        &plaintextLen)) {
                return false;
              }
              if (adoptAltKey) {
                memcpy(connectionEncSessionKey_, connectionEncSessionKeyAlt_,
                       sizeof(connectionEncSessionKey_));
                connectionEncAltKeyValid_ = false;
                memcpy(encDebug_.encLastSessionKey, connectionEncSessionKey_,
                       sizeof(encDebug_.encLastSessionKey));
                encDebug_.encLastSessionKeyValid = 1U;
              }
              if (swapDirection) {
                connectionEncRxDirection_ = rxDirection;
                connectionEncTxDirection_ =
                    static_cast<uint8_t>(rxDirection ^ 0x01U);
                encDebug_.encLastRxDir = connectionEncRxDirection_;
                encDebug_.encLastTxDir = connectionEncTxDirection_;
              }
              rxLengthFollow = plaintextLen;
              if (plaintextLen > 0U) {
                memcpy(&rxPacket_[2], plaintext, plaintextLen);
              }
              if (packetIsNewFollow) {
                connectionEncRxCounter_ =
                    (connectionEncRxCounter_ + 1ULL) & kBleEncPacketCounterMask;
              }
              return true;
            };

        const uint8_t swappedRxDirection =
            static_cast<uint8_t>(connectionEncRxDirection_ ^ 0x01U);
        const bool decrypted =
            tryDecryptFollowEncryptedPdu(connectionEncSessionKey_,
                                         connectionEncRxDirection_, false, false) ||
            tryDecryptFollowEncryptedPdu(connectionEncSessionKey_,
                                         swappedRxDirection, false, true) ||
            (connectionEncAltKeyValid_ &&
             tryDecryptFollowEncryptedPdu(connectionEncSessionKeyAlt_,
                                          connectionEncRxDirection_, true, false)) ||
            (connectionEncAltKeyValid_ &&
             tryDecryptFollowEncryptedPdu(connectionEncSessionKeyAlt_,
                                          swappedRxDirection, true, true));
        if (!decrypted) {
          terminateInd = true;
          break;
        }
      }

      bool implicitEmptyAckFollow = false;
      const bool lastTxWasEmptyAckFollow =
          connectionTxHistoryValid_ &&
          (connectionLastTxLlid_ == 0x01U) &&
          (connectionLastTxLength_ == 0U);
      const bool lastTxHadAttPayloadFollow =
          connectionTxHistoryValid_ &&
          (connectionLastTxLlid_ == kBlePduDataStartOrComplete) &&
          (connectionLastTxPlainLength_ >= (kBleL2capHeaderLen + 1U)) &&
          (readLe16(&connectionLastTxPlainPayload_[2]) == kBleL2capCidAtt);
      const bool peerAdvancedToNewAttServerProgressFollow =
          packetIsNewFollow &&
          (llidFollow == kBlePduDataStartOrComplete) &&
          (rxLengthFollow >= (kBleL2capHeaderLen + 1U)) &&
          (readLe16(&rxPacket_[4]) == kBleL2capCidAtt) &&
          (attOpcodeIsServerResponsePdu(rxPacket_[2 + kBleL2capHeaderLen]) ||
           (rxPacket_[2 + kBleL2capHeaderLen] == kAttOpHandleValueNtf) ||
           (rxPacket_[2 + kBleL2capHeaderLen] == kAttOpHandleValueInd));
      if (!peerAckedLastTxFollow &&
          connectionTxHistoryValid_ &&
          packetIsNewFollow &&
          ((lastTxWasEmptyAckFollow && peerAdvancedToNewAttServerProgressFollow) ||
           (lastTxHadAttPayloadFollow && peerAdvancedToNewAttServerProgressFollow))) {
        peerAckedLastTxFollow = true;
        implicitEmptyAckFollow = true;
        if (lastTxHadAttPayloadFollow &&
            peerAdvancedToNewAttServerProgressFollow) {
          ++encDebug_.connImplicitAttProgressAckCount;
        }
      }
      if (peerAckedLastTxFollow) {
        connectionTxSn_ ^= 0x01U;
        connectionFreshTxAllowed_ = true;
      } else {
        connectionFreshTxAllowed_ = false;
      }

      if ((llidFollow == kBlePduLlControl) &&
          (rxLengthFollow >= 2U) &&
          (rxPacket_[2] == kBleLlCtrlTerminateInd)) {
        terminateInd = true;
        peerTerminateIndReceived = true;
        peerTerminateErrorCode = rxPacket_[3];
      }

      rememberConnectionRxDebug(
          llidFollow, rxLengthFollow,
          bleConnectionPayloadProtocolOpcode(llidFollow, &rxPacket_[2], rxLengthFollow),
          nesnFollow, snFollow, packetIsNewFollow, peerAckedLastTxFollow);

      const bool rxHasL2capFollow =
          packetIsNewFollow &&
          (llidFollow == kBlePduDataStartOrComplete) &&
          (rxLengthFollow >= kBleL2capHeaderLen);
      const uint16_t rxL2capLenFollow =
          rxHasL2capFollow ? readLe16(&rxPacket_[2]) : 0U;
      const uint16_t rxL2capCidFollow =
          rxHasL2capFollow ? readLe16(&rxPacket_[4]) : 0U;
      const uint8_t rxAttOpcodeFollow =
          (rxHasL2capFollow && rxLengthFollow > kBleL2capHeaderLen)
              ? rxPacket_[2 + kBleL2capHeaderLen]
              : 0U;

      if (packetIsNewFollow &&
          (llidFollow == kBlePduLlControl) &&
          (rxLengthFollow >= 1U)) {
        const uint8_t opcode = rxPacket_[2];
        uint8_t deferredLlControlResponse[kBleDataPduMaxPayload];
        if (opcode == kBleLlCtrlRejectInd && rxLengthFollow >= 2U &&
            rxPacket_[3] == kBleLlCtrlLengthReq) {
          connectionDataLengthUpdatePending_ = false;
          connectionDataLengthUpdateInFlight_ = false;
        } else if (opcode == kBleLlCtrlRejectExtInd && rxLengthFollow >= 3U &&
                   rxPacket_[3] == kBleLlCtrlLengthReq) {
          connectionDataLengthUpdatePending_ = false;
          connectionDataLengthUpdateInFlight_ = false;
        }

        if (!terminateInd) {
          uint8_t responseLength = 0U;
          bool responseTerminate = false;
          uint8_t* responseBuffer =
              connectionPendingTxValid_ ? deferredLlControlResponse
                                        : connectionPendingTxPayload_;
          if (buildLlControlResponse(&rxPacket_[2], rxLengthFollow,
                                     currentEventCounter, responseBuffer,
                                     &responseLength, &responseTerminate) &&
              responseLength > 0U &&
              !connectionPendingTxValid_) {
            connectionPendingTxLlid_ = kBlePduLlControl;
            connectionPendingTxLength_ = responseLength;
            connectionPendingTxValid_ = true;
          }
          if (responseTerminate) {
            terminateInd = true;
          }
        }
      }

      if (rxHasL2capFollow &&
          (rxL2capCidFollow == kBleL2capCidAtt) &&
          (rxL2capLenFollow >= 3U) &&
          (rxAttOpcodeFollow == kAttOpExchangeMtuRsp)) {
        const uint16_t serverRxMtu =
            readLe16(&rxPacket_[2 + kBleL2capHeaderLen + 1U]);
        uint16_t localRxMtu = connectionRequestedAttMtu_;
        if (localRxMtu < kBleDefaultAttMtu) {
          localRxMtu = kBleDefaultAttMtu;
        } else if (localRxMtu > localPreferredAttMtu()) {
          localRxMtu = localPreferredAttMtu();
        }
        const uint16_t negotiatedMtu = minU16(serverRxMtu, localRxMtu);
        connectionAttMtu_ = (negotiatedMtu >= kBleDefaultAttMtu)
                                ? negotiatedMtu
                                : kBleDefaultAttMtu;
        connectionAttMtuExchangePending_ = false;
        connectionAttMtuExchangeInFlight_ = false;
        connectionAttMtuExchangeComplete_ = true;
      }

      if (rxHasL2capFollow &&
          !terminateInd &&
          !connectionPendingTxValid_) {
        uint8_t responseLength = 0U;
        if (buildL2capResponse(llidFollow, &rxPacket_[2], rxLengthFollow,
                               packetIsNewFollow, connectionPendingTxPayload_,
                               &responseLength) &&
            responseLength > 0U) {
          connectionPendingTxLlid_ = kBlePduDataStartOrComplete;
          connectionPendingTxLength_ = responseLength;
          connectionPendingTxValid_ = true;
        }
      }

      if (rxHasL2capFollow &&
          (rxL2capCidFollow == kBleL2capCidAtt) &&
          (rxL2capLenFollow > 0U) &&
          (rxAttOpcodeFollow == kAttOpHandleValueInd) &&
          !connectionPendingTxValid_) {
        writeLe16(&connectionPendingTxPayload_[0], 1U);
        writeLe16(&connectionPendingTxPayload_[2], kBleL2capCidAtt);
        connectionPendingTxPayload_[4] = kAttOpHandleValueCfm;
        connectionPendingTxLlid_ = kBlePduDataStartOrComplete;
        connectionPendingTxLength_ = 5U;
        connectionPendingTxValid_ = true;
      }

      maybeQueueCentralLinkSetupRequest();
      const bool followRxIsAttServerPush =
          rxHasL2capFollow &&
          (rxL2capCidFollow == kBleL2capCidAtt) &&
          ((rxAttOpcodeFollow == kAttOpHandleValueNtf) ||
           (rxAttOpcodeFollow == kAttOpHandleValueInd));
      const bool followRxIsAttServerIndication =
          rxHasL2capFollow &&
          (rxL2capCidFollow == kBleL2capCidAtt) &&
          (rxAttOpcodeFollow == kAttOpHandleValueInd);
      const bool followRxIsAttServerResponse =
          rxHasL2capFollow &&
          (rxL2capCidFollow == kBleL2capCidAtt) &&
          attOpcodeIsServerResponsePdu(rxAttOpcodeFollow);
      followMustAckPeerServerPush = followRxIsAttServerIndication;
      currentFollowupPeerMoreData = (((hdrFollow >> 4U) & 0x01U) != 0U);
      keepTurningAround =
          !terminateInd &&
          (followRxIsAttServerPush ||
           pendingTxIsHandleValueCfm() ||
           (followRxIsAttServerResponse && currentFollowupPeerMoreData) ||
           (connectionFreshTxAllowed_ && connectionPendingTxValid_));
    }

    clearRadioCoreEvents(radio_);
    if (restoreOriginalPacketAfterTurnaround) {
      rxPacket_[0] = hdr0;
      rxPacket_[1] = rxLength;
      memcpy(&rxPacket_[2], originalRxPayload, rxLength);
    }
  }

  if (event != nullptr) {
    event->txPacketSent = true;
    event->txLlid = txLlid;
    event->txNesn = txNesnBit;
    event->txSn = txSnBit;
    event->txPayloadLength = txLength;
    event->txPayload =
        (txLength > 0U) ? &connectionLastTxPlainPayload_[0] : nullptr;
    event->emptyAckTransmitted = (txLlid == 0x01U) && (txLength == 0U);
    event->terminateInd = terminateInd;
  }

  if (terminateInd) {
    emitBleTrace("CENTRAL_TERMINATE");
    disconnectWithReason(BleDisconnectReason::kPeerTerminate,
                         peerTerminateErrorCode, spinLimit / 2U + 1U);
  }

  clearRadioCoreEvents(radio_);
  return true;
}
