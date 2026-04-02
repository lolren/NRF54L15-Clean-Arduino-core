#ifndef NRF54L15_TINYUSB_HID_H_
#define NRF54L15_TINYUSB_HID_H_

#include <stdint.h>

#ifndef TU_ATTR_PACKED
#define TU_ATTR_PACKED __attribute__((packed))
#endif

#ifndef TU_BIT
#define TU_BIT(n) (1UL << (n))
#endif

typedef struct TU_ATTR_PACKED {
  int8_t x;
  int8_t y;
  int8_t z;
  int8_t rz;
  int8_t rx;
  int8_t ry;
  uint8_t hat;
  uint32_t buttons;
} hid_gamepad_report_t;

typedef enum {
  GAMEPAD_HAT_CENTERED = 0,
  GAMEPAD_HAT_UP = 1,
  GAMEPAD_HAT_UP_RIGHT = 2,
  GAMEPAD_HAT_RIGHT = 3,
  GAMEPAD_HAT_DOWN_RIGHT = 4,
  GAMEPAD_HAT_DOWN = 5,
  GAMEPAD_HAT_DOWN_LEFT = 6,
  GAMEPAD_HAT_LEFT = 7,
  GAMEPAD_HAT_UP_LEFT = 8,
} hid_gamepad_hat_t;

typedef struct TU_ATTR_PACKED {
  uint8_t buttons;
  int8_t x;
  int8_t y;
  int8_t wheel;
  int8_t pan;
} hid_mouse_report_t;

typedef enum {
  MOUSE_BUTTON_LEFT = TU_BIT(0),
  MOUSE_BUTTON_RIGHT = TU_BIT(1),
  MOUSE_BUTTON_MIDDLE = TU_BIT(2),
  MOUSE_BUTTON_BACKWARD = TU_BIT(3),
  MOUSE_BUTTON_FORWARD = TU_BIT(4),
} hid_mouse_button_bm_t;

typedef struct TU_ATTR_PACKED {
  uint8_t modifier;
  uint8_t reserved;
  uint8_t keycode[6];
} hid_keyboard_report_t;

typedef enum {
  KEYBOARD_MODIFIER_LEFTCTRL = TU_BIT(0),
  KEYBOARD_MODIFIER_LEFTSHIFT = TU_BIT(1),
  KEYBOARD_MODIFIER_LEFTALT = TU_BIT(2),
  KEYBOARD_MODIFIER_LEFTGUI = TU_BIT(3),
  KEYBOARD_MODIFIER_RIGHTCTRL = TU_BIT(4),
  KEYBOARD_MODIFIER_RIGHTSHIFT = TU_BIT(5),
  KEYBOARD_MODIFIER_RIGHTALT = TU_BIT(6),
  KEYBOARD_MODIFIER_RIGHTGUI = TU_BIT(7),
} hid_keyboard_modifier_bm_t;

typedef enum {
  KEYBOARD_LED_NUMLOCK = TU_BIT(0),
  KEYBOARD_LED_CAPSLOCK = TU_BIT(1),
  KEYBOARD_LED_SCROLLLOCK = TU_BIT(2),
  KEYBOARD_LED_COMPOSE = TU_BIT(3),
  KEYBOARD_LED_KANA = TU_BIT(4),
} hid_keyboard_led_bm_t;

#define HID_KEY_NONE 0x00
#define HID_KEY_A 0x04
#define HID_KEY_B 0x05
#define HID_KEY_C 0x06
#define HID_KEY_D 0x07
#define HID_KEY_E 0x08
#define HID_KEY_F 0x09
#define HID_KEY_G 0x0A
#define HID_KEY_H 0x0B
#define HID_KEY_I 0x0C
#define HID_KEY_J 0x0D
#define HID_KEY_K 0x0E
#define HID_KEY_L 0x0F
#define HID_KEY_M 0x10
#define HID_KEY_N 0x11
#define HID_KEY_O 0x12
#define HID_KEY_P 0x13
#define HID_KEY_Q 0x14
#define HID_KEY_R 0x15
#define HID_KEY_S 0x16
#define HID_KEY_T 0x17
#define HID_KEY_U 0x18
#define HID_KEY_V 0x19
#define HID_KEY_W 0x1A
#define HID_KEY_X 0x1B
#define HID_KEY_Y 0x1C
#define HID_KEY_Z 0x1D
#define HID_KEY_1 0x1E
#define HID_KEY_2 0x1F
#define HID_KEY_3 0x20
#define HID_KEY_4 0x21
#define HID_KEY_5 0x22
#define HID_KEY_6 0x23
#define HID_KEY_7 0x24
#define HID_KEY_8 0x25
#define HID_KEY_9 0x26
#define HID_KEY_0 0x27
#define HID_KEY_ENTER 0x28
#define HID_KEY_ESCAPE 0x29
#define HID_KEY_BACKSPACE 0x2A
#define HID_KEY_TAB 0x2B
#define HID_KEY_SPACE 0x2C
#define HID_KEY_MINUS 0x2D
#define HID_KEY_EQUAL 0x2E
#define HID_KEY_BRACKET_LEFT 0x2F
#define HID_KEY_BRACKET_RIGHT 0x30
#define HID_KEY_BACKSLASH 0x31
#define HID_KEY_NON_US_HASH 0x32
#define HID_KEY_SEMICOLON 0x33
#define HID_KEY_APOSTROPHE 0x34
#define HID_KEY_GRAVE 0x35
#define HID_KEY_COMMA 0x36
#define HID_KEY_PERIOD 0x37
#define HID_KEY_SLASH 0x38
#define HID_KEY_KEYPAD_DIVIDE 0x54
#define HID_KEY_KEYPAD_MULTIPLY 0x55
#define HID_KEY_KEYPAD_SUBTRACT 0x56
#define HID_KEY_KEYPAD_ADD 0x57
#define HID_KEY_KEYPAD_ENTER 0x58
#define HID_KEY_KEYPAD_1 0x59
#define HID_KEY_KEYPAD_2 0x5A
#define HID_KEY_KEYPAD_3 0x5B
#define HID_KEY_KEYPAD_4 0x5C
#define HID_KEY_KEYPAD_5 0x5D
#define HID_KEY_KEYPAD_6 0x5E
#define HID_KEY_KEYPAD_7 0x5F
#define HID_KEY_KEYPAD_8 0x60
#define HID_KEY_KEYPAD_9 0x61
#define HID_KEY_KEYPAD_0 0x62
#define HID_KEY_KEYPAD_DECIMAL 0x63

typedef enum {
  HID_USAGE_CONSUMER_VOLUME_DECREMENT = 0x00EA,
} hid_usage_consumer_enum_t;

#endif  // NRF54L15_TINYUSB_HID_H_
