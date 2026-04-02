#ifndef BONDING_H_
#define BONDING_H_

#include "bluefruit_common.h"

struct bond_keys_t {
  uint8_t data[80];
};

inline void bond_init(void) {}
inline void bond_clear_prph(void) {}
inline void bond_clear_cntr(void) {}
inline void bond_clear_all(void) {}
inline void bond_remove_key(uint8_t role, ble_gap_addr_t const* id_addr) {
  (void)role;
  (void)id_addr;
}
inline bool bond_save_keys(uint8_t role, uint16_t conn_hdl, bond_keys_t const* bkeys) {
  (void)role;
  (void)conn_hdl;
  (void)bkeys;
  return false;
}
inline bool bond_load_keys(uint8_t role, ble_gap_addr_t* peer_addr, bond_keys_t* bkeys) {
  (void)role;
  (void)peer_addr;
  (void)bkeys;
  return false;
}
inline bool bond_save_cccd(uint8_t role, uint16_t conn_hdl, ble_gap_addr_t const* id_addr) {
  (void)role;
  (void)conn_hdl;
  (void)id_addr;
  return false;
}
inline bool bond_load_cccd(uint8_t role, uint16_t conn_hdl, ble_gap_addr_t const* id_addr) {
  (void)role;
  (void)conn_hdl;
  (void)id_addr;
  return false;
}
inline void bond_print_list(uint8_t role) { (void)role; }

#endif  // BONDING_H_
