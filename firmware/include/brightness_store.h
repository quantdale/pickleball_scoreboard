#ifndef BRIGHTNESS_STORE_H
#define BRIGHTNESS_STORE_H

#include <stdint.h>

// Spec 03 §2a: one byte in NVS namespace "pksdisp", apart from OTA credentials.
// Load never fails: a missing or invalid record restores the default level.
uint8_t loadBrightnessLevel();
bool storeBrightnessLevel(uint8_t level);

#endif
