#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "nrf54l15_vpr_transport_shared.h"

#define VPRCSR_NORDIC_VPRNORDICCTRL 0x7C0U
#define VPRCSR_NORDIC_VPRNORDICCTRL_ENABLERTPERIPH_Msk (1UL << 0U)
#define VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Pos 16U
#define VPRCSR_NORDIC_VPRNORDICCTRL_NORDICKEY_Enabled 0x507DUL

#define BLE_CS_MAIN_MODE2 0x02U

#define BLE_CS_HCI_OP_READ_REMOTE_SUPPORTED_CAPABILITIES 0x208AU
#define BLE_CS_HCI_OP_SECURITY_ENABLE 0x208CU
#define BLE_CS_HCI_OP_SET_DEFAULT_SETTINGS 0x208DU
#define BLE_CS_HCI_OP_CREATE_CONFIG 0x2090U
#define BLE_CS_HCI_OP_SET_PROCEDURE_PARAMETERS 0x2093U
#define BLE_CS_HCI_OP_PROCEDURE_ENABLE 0x2094U

#define BLE_CS_HCI_EVT_READ_REMOTE_SUPPORTED_CAPS_COMPLETE_V2 0x38U
#define BLE_CS_HCI_EVT_SECURITY_ENABLE_COMPLETE 0x2EU
#define BLE_CS_HCI_EVT_CONFIG_COMPLETE 0x2FU
#define BLE_CS_HCI_EVT_PROCEDURE_ENABLE_COMPLETE 0x30U
#define BLE_CS_HCI_EVT_SUBEVENT_RESULT 0x31U
#define BLE_CS_HCI_EVT_SUBEVENT_RESULT_CONTINUE 0x32U

#define BLE_HCI_PACKET_TYPE_EVENT 0x04U
#define BLE_HCI_EVT_COMMAND_COMPLETE 0x0EU
#define BLE_HCI_EVT_COMMAND_STATUS 0x0FU
#define BLE_HCI_EVT_LE_META 0x3EU

extern uint8_t __stack_top[];

static volatile Nrf54l15VprTransportHostShared* const g_host_transport =
    (volatile Nrf54l15VprTransportHostShared*)(uintptr_t)NRF54L15_VPR_TRANSPORT_HOST_BASE;
static volatile Nrf54l15VprTransportVprShared* const g_vpr_transport =
    (volatile Nrf54l15VprTransportVprShared*)(uintptr_t)NRF54L15_VPR_TRANSPORT_VPR_BASE;

static inline void fence_rw(void) {
  __asm__ volatile("fence rw, rw" ::: "memory");
}

static void bytes_zero(void *dst, size_t len) {
  uint8_t *out = (uint8_t *)dst;
  if (out == NULL) {
    return;
  }
  for (size_t i = 0U; i < len; ++i) {
    out[i] = 0U;
  }
}

static void bytes_copy(void *dst, const void *src, size_t len) {
  uint8_t *out = (uint8_t *)dst;
  const uint8_t *in = (const uint8_t *)src;
  if (out == NULL || in == NULL) {
    return;
  }
  for (size_t i = 0U; i < len; ++i) {
    out[i] = in[i];
  }
}

static void zero_vpr_data(void) {
  for (uint32_t i = 0U; i < NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA; ++i) {
    g_vpr_transport->vprData[i] = 0U;
  }
}

static void write_le16(uint8_t *dst, uint16_t value) {
  if (dst == NULL) {
    return;
  }
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
}

static void write_le24(uint8_t *dst, uint32_t value) {
  if (dst == NULL) {
    return;
  }
  dst[0] = (uint8_t)(value & 0xFFU);
  dst[1] = (uint8_t)((value >> 8U) & 0xFFU);
  dst[2] = (uint8_t)((value >> 16U) & 0xFFU);
}

static uint16_t read_conn_handle(void) {
  if (g_host_transport->hostLen < 6U) {
    return 0x0040U;
  }
  return (uint16_t)g_host_transport->hostData[4] |
         ((uint16_t)g_host_transport->hostData[5] << 8U);
}

static size_t append_h4_command_status(uint8_t *dst, size_t max_len, uint16_t opcode,
                                       uint8_t status) {
  if (dst == NULL || max_len < 7U) {
    return 0U;
  }
  dst[0] = BLE_HCI_PACKET_TYPE_EVENT;
  dst[1] = BLE_HCI_EVT_COMMAND_STATUS;
  dst[2] = 4U;
  dst[3] = status;
  dst[4] = 1U;
  write_le16(&dst[5], opcode);
  return 7U;
}

static size_t append_h4_command_complete(uint8_t *dst, size_t max_len, uint16_t opcode,
                                         uint8_t status) {
  if (dst == NULL || max_len < 7U) {
    return 0U;
  }
  dst[0] = BLE_HCI_PACKET_TYPE_EVENT;
  dst[1] = BLE_HCI_EVT_COMMAND_COMPLETE;
  dst[2] = 4U;
  dst[3] = 1U;
  write_le16(&dst[4], opcode);
  dst[6] = status;
  return 7U;
}

static size_t append_h4_le_meta(uint8_t *dst, size_t max_len, uint8_t subevent_code,
                                const uint8_t *payload, size_t payload_len) {
  if (dst == NULL || payload == NULL || max_len < (4U + payload_len)) {
    return 0U;
  }
  dst[0] = BLE_HCI_PACKET_TYPE_EVENT;
  dst[1] = BLE_HCI_EVT_LE_META;
  dst[2] = (uint8_t)(1U + payload_len);
  dst[3] = subevent_code;
  bytes_copy(&dst[4], payload, payload_len);
  return 4U + payload_len;
}

static void append_mode2_demo_step(uint8_t *dst, uint8_t channel) {
  if (dst == NULL) {
    return;
  }
  dst[0] = BLE_CS_MAIN_MODE2;
  dst[1] = channel;
  dst[2] = 5U;
  dst[3] = 0U;
  dst[4] = 0x00U;
  dst[5] = 0x04U;
  dst[6] = 0x00U;
  dst[7] = 0x00U;
}

static size_t build_remote_caps_payload(uint8_t *payload, size_t max_len, uint16_t conn_handle) {
  if (payload == NULL || max_len < 34U) {
    return 0U;
  }
  bytes_zero(payload, 34U);
  payload[0] = 0U;
  write_le16(&payload[1], conn_handle);
  payload[3] = 4U;
  write_le16(&payload[4], 6U);
  payload[6] = 4U;
  payload[7] = 4U;
  payload[8] = 0x03U;
  payload[9] = 0x01U;
  payload[10] = 0x07U;
  payload[11] = 0x02U;
  payload[12] = 0x03U;
  payload[13] = 0x04U;
  write_le16(&payload[14], 0x0001U);
  write_le16(&payload[16], 0x0001U);
  payload[18] = 0x06U;
  write_le16(&payload[19], 0x001EU);
  write_le16(&payload[21], 10U);
  write_le16(&payload[23], 20U);
  write_le16(&payload[25], 30U);
  write_le16(&payload[27], 40U);
  payload[29] = 3U;
  payload[30] = 4U;
  write_le16(&payload[31], 50U);
  payload[33] = 6U;
  return 34U;
}

static size_t build_security_complete_payload(uint8_t *payload, size_t max_len,
                                              uint16_t conn_handle) {
  if (payload == NULL || max_len < 3U) {
    return 0U;
  }
  payload[0] = 0U;
  write_le16(&payload[1], conn_handle);
  return 3U;
}

static size_t build_config_complete_payload(uint8_t *payload, size_t max_len,
                                            uint16_t conn_handle) {
  static const uint8_t channel_map[10] = {0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x1FU,
                                          0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  if (payload == NULL || max_len < 33U) {
    return 0U;
  }
  bytes_zero(payload, 33U);
  payload[0] = 0U;
  write_le16(&payload[1], conn_handle);
  payload[3] = 1U;
  payload[4] = 1U;
  payload[5] = BLE_CS_MAIN_MODE2;
  payload[6] = 0xFFU;
  payload[7] = 3U;
  payload[8] = 5U;
  payload[9] = 1U;
  payload[10] = 1U;
  payload[11] = 0U;
  payload[12] = 1U;
  payload[13] = 2U;
  bytes_copy(&payload[14], channel_map, sizeof(channel_map));
  payload[24] = 1U;
  payload[25] = 1U;
  payload[26] = 1U;
  payload[27] = 3U;
  payload[28] = 0x01U;
  payload[29] = 10U;
  payload[30] = 20U;
  payload[31] = 30U;
  payload[32] = 40U;
  return 33U;
}

static size_t build_procedure_enable_complete_payload(uint8_t *payload, size_t max_len,
                                                      uint16_t conn_handle) {
  if (payload == NULL || max_len < 21U) {
    return 0U;
  }
  bytes_zero(payload, 21U);
  payload[0] = 0U;
  write_le16(&payload[1], conn_handle);
  payload[3] = 1U;
  payload[4] = 1U;
  payload[5] = 2U;
  payload[6] = (uint8_t)(int8_t)-12;
  write_le24(&payload[7], 0x000456UL);
  payload[10] = 4U;
  write_le16(&payload[11], 100U);
  write_le16(&payload[13], 200U);
  write_le16(&payload[15], 300U);
  write_le16(&payload[17], 8U);
  write_le16(&payload[19], 12U);
  return 21U;
}

static size_t build_subevent_initial_payload(uint8_t *payload, size_t max_len,
                                             uint16_t conn_handle) {
  if (payload == NULL || max_len < 31U) {
    return 0U;
  }
  bytes_zero(payload, 31U);
  write_le16(&payload[0], conn_handle);
  payload[2] = 1U;
  write_le16(&payload[3], 0x1234U);
  write_le16(&payload[5], 7U);
  write_le16(&payload[7], 0U);
  payload[9] = 0U;
  payload[10] = 0x01U;
  payload[11] = 0x01U;
  payload[12] = 0U;
  payload[13] = 2U;
  payload[14] = 2U;
  append_mode2_demo_step(&payload[15], 0U);
  append_mode2_demo_step(&payload[23], 12U);
  return 31U;
}

static size_t build_subevent_continue_payload(uint8_t *payload, size_t max_len,
                                              uint16_t conn_handle) {
  if (payload == NULL || max_len < 24U) {
    return 0U;
  }
  bytes_zero(payload, 24U);
  write_le16(&payload[0], conn_handle);
  payload[2] = 1U;
  payload[3] = 0U;
  payload[4] = 0U;
  payload[5] = 0U;
  payload[6] = 2U;
  payload[7] = 2U;
  append_mode2_demo_step(&payload[8], 24U);
  append_mode2_demo_step(&payload[16], 36U);
  return 24U;
}

static uint16_t read_opcode(void) {
  if (g_host_transport->hostLen < 4U) {
    return 0U;
  }
  if (g_host_transport->hostData[0] != 0x01U) {
    return 0U;
  }
  return (uint16_t)g_host_transport->hostData[1] |
         ((uint16_t)g_host_transport->hostData[2] << 8U);
}

static void build_unknown_command_response(uint16_t opcode) {
  zero_vpr_data();
  g_vpr_transport->vprData[0] = 0x04U;
  g_vpr_transport->vprData[1] = 0x0EU;
  g_vpr_transport->vprData[2] = 4U;
  g_vpr_transport->vprData[3] = 1U;
  g_vpr_transport->vprData[4] = (uint8_t)(opcode & 0xFFU);
  g_vpr_transport->vprData[5] = (uint8_t)((opcode >> 8U) & 0xFFU);
  g_vpr_transport->vprData[6] = 0x01U;
  g_vpr_transport->vprLen = 7U;
}

static bool publish_builtin_response_for_opcode(uint16_t opcode) {
  uint8_t payload[40];
  uint16_t conn_handle = read_conn_handle();
  size_t offset = 0U;
  zero_vpr_data();

  switch (opcode) {
    case BLE_CS_HCI_OP_READ_REMOTE_SUPPORTED_CAPABILITIES: {
      size_t len = append_h4_command_status((uint8_t *)g_vpr_transport->vprData + offset,
                                            NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                                            opcode, 0U);
      if (len == 0U) {
        return false;
      }
      offset += len;
      len = build_remote_caps_payload(payload, sizeof(payload), conn_handle);
      if (len == 0U) {
        return false;
      }
      len = append_h4_le_meta((uint8_t *)g_vpr_transport->vprData + offset,
                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                              BLE_CS_HCI_EVT_READ_REMOTE_SUPPORTED_CAPS_COMPLETE_V2, payload, len);
      if (len == 0U) {
        return false;
      }
      offset += len;
      break;
    }
    case BLE_CS_HCI_OP_SET_DEFAULT_SETTINGS: {
      size_t len = append_h4_command_complete((uint8_t *)g_vpr_transport->vprData + offset,
                                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                                              opcode, 0U);
      if (len == 0U) {
        return false;
      }
      offset += len;
      break;
    }
    case BLE_CS_HCI_OP_CREATE_CONFIG: {
      size_t len = append_h4_command_status((uint8_t *)g_vpr_transport->vprData + offset,
                                            NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                                            opcode, 0U);
      if (len == 0U) {
        return false;
      }
      offset += len;
      len = build_config_complete_payload(payload, sizeof(payload), conn_handle);
      if (len == 0U) {
        return false;
      }
      len = append_h4_le_meta((uint8_t *)g_vpr_transport->vprData + offset,
                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                              BLE_CS_HCI_EVT_CONFIG_COMPLETE, payload, len);
      if (len == 0U) {
        return false;
      }
      offset += len;
      break;
    }
    case BLE_CS_HCI_OP_SECURITY_ENABLE: {
      size_t len = append_h4_command_status((uint8_t *)g_vpr_transport->vprData + offset,
                                            NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                                            opcode, 0U);
      if (len == 0U) {
        return false;
      }
      offset += len;
      len = build_security_complete_payload(payload, sizeof(payload), conn_handle);
      if (len == 0U) {
        return false;
      }
      len = append_h4_le_meta((uint8_t *)g_vpr_transport->vprData + offset,
                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                              BLE_CS_HCI_EVT_SECURITY_ENABLE_COMPLETE, payload, len);
      if (len == 0U) {
        return false;
      }
      offset += len;
      break;
    }
    case BLE_CS_HCI_OP_SET_PROCEDURE_PARAMETERS: {
      size_t len = append_h4_command_complete((uint8_t *)g_vpr_transport->vprData + offset,
                                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                                              opcode, 0U);
      if (len == 0U) {
        return false;
      }
      offset += len;
      break;
    }
    case BLE_CS_HCI_OP_PROCEDURE_ENABLE: {
      size_t len = append_h4_command_status((uint8_t *)g_vpr_transport->vprData + offset,
                                            NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                                            opcode, 0U);
      if (len == 0U) {
        return false;
      }
      offset += len;
      len = build_procedure_enable_complete_payload(payload, sizeof(payload), conn_handle);
      if (len == 0U) {
        return false;
      }
      len = append_h4_le_meta((uint8_t *)g_vpr_transport->vprData + offset,
                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                              BLE_CS_HCI_EVT_PROCEDURE_ENABLE_COMPLETE, payload, len);
      if (len == 0U) {
        return false;
      }
      offset += len;
      len = build_subevent_initial_payload(payload, sizeof(payload), conn_handle);
      if (len == 0U) {
        return false;
      }
      len = append_h4_le_meta((uint8_t *)g_vpr_transport->vprData + offset,
                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                              BLE_CS_HCI_EVT_SUBEVENT_RESULT, payload, len);
      if (len == 0U) {
        return false;
      }
      offset += len;
      len = build_subevent_continue_payload(payload, sizeof(payload), conn_handle);
      if (len == 0U) {
        return false;
      }
      len = append_h4_le_meta((uint8_t *)g_vpr_transport->vprData + offset,
                              NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA - offset,
                              BLE_CS_HCI_EVT_SUBEVENT_RESULT_CONTINUE, payload, len);
      if (len == 0U) {
        return false;
      }
      offset += len;
      break;
    }
    default:
      return false;
  }

  g_vpr_transport->vprLen = (uint32_t)offset;
  return true;
}

static void publish_response_for_opcode(uint16_t opcode) {
  const uint32_t count =
      (g_host_transport->scriptCount <= NRF54L15_VPR_TRANSPORT_MAX_SCRIPT_COUNT)
          ? g_host_transport->scriptCount
          : NRF54L15_VPR_TRANSPORT_MAX_SCRIPT_COUNT;

  g_vpr_transport->lastOpcode = (uint32_t)opcode;
  for (uint32_t i = 0U; i < count; ++i) {
    volatile const Nrf54l15VprTransportScript* script = &g_host_transport->scripts[i];
    if (script->opcode != opcode) {
      continue;
    }

    if (script->responseLen > NRF54L15_VPR_TRANSPORT_MAX_VPR_DATA) {
      g_vpr_transport->lastError = 2U;
      g_vpr_transport->status = NRF54L15_VPR_TRANSPORT_STATUS_ERROR;
      g_vpr_transport->vprLen = 0U;
      return;
    }

    zero_vpr_data();
    bytes_copy((void *)g_vpr_transport->vprData, (const void *)script->response,
               script->responseLen);
    g_vpr_transport->vprLen = script->responseLen;
    return;
  }

  if (!publish_builtin_response_for_opcode(opcode)) {
    build_unknown_command_response(opcode);
  }
}

static bool host_request_pending(void) {
  return ((g_host_transport->hostFlags & NRF54L15_VPR_TRANSPORT_FLAG_PENDING) != 0U) ||
         (g_host_transport->hostLen != 0U);
}

static bool consume_host_request(uint32_t host_seq) {
  if (!host_request_pending()) {
    return false;
  }

  const uint16_t opcode = read_opcode();
  publish_response_for_opcode(opcode);
  g_vpr_transport->vprSeq = host_seq;
  g_vpr_transport->vprFlags = NRF54L15_VPR_TRANSPORT_FLAG_PENDING;
  g_host_transport->hostFlags = 0U;
  g_host_transport->hostLen = 0U;
  g_vpr_transport->status = NRF54L15_VPR_TRANSPORT_STATUS_READY;
  fence_rw();
  return true;
}

__attribute__((noreturn)) void vpr_main(void) {
  uint32_t last_seq = 0U;

  g_host_transport->magic = NRF54L15_VPR_TRANSPORT_MAGIC;
  g_host_transport->version = NRF54L15_VPR_TRANSPORT_VERSION;
  g_vpr_transport->magic = NRF54L15_VPR_TRANSPORT_MAGIC;
  g_vpr_transport->version = NRF54L15_VPR_TRANSPORT_VERSION;
  g_vpr_transport->status = NRF54L15_VPR_TRANSPORT_STATUS_READY;
  g_vpr_transport->heartbeat = 0U;
  g_vpr_transport->vprFlags = 0U;
  g_vpr_transport->vprLen = 0U;
  g_vpr_transport->lastError = 0U;
  fence_rw();

  while (1) {
    g_vpr_transport->heartbeat = g_vpr_transport->heartbeat + 1U;
    fence_rw();

    const uint32_t host_seq = g_host_transport->hostSeq;
    const uint32_t host_flags = g_host_transport->hostFlags;
    g_vpr_transport->reserved =
        ((host_seq & 0xFFFFU) << 16U) |
        ((host_flags & NRF54L15_VPR_TRANSPORT_FLAG_PENDING) != 0U ? 0x2U : 0U) |
        0x4U;
    fence_rw();

    if ((host_seq != last_seq) && host_request_pending()) {
      if (consume_host_request(host_seq)) {
        last_seq = host_seq;
      }
    }
  }
}

__attribute__((naked, noreturn, section(".text.start"))) void _start(void) {
  __asm__ volatile(
      "li gp, 0\n"
      "la sp, __stack_top\n"
      "j vpr_main\n");
}
