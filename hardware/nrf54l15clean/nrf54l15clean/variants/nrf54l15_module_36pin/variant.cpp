#include "Arduino.h"
#include "variant.h"

extern "C" void initVariant(void) {
    // Bare module variants do not require board-specific rail bring-up.
}
