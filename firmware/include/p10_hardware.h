#ifndef P10_HARDWARE_H
#define P10_HARDWARE_H

#include <stdint.h>

// Spec 03 Section 2: accepted bring-up values; keep diagnostics independent.
constexpr uint8_t P10_BRIGHTNESS = 12;
constexpr int16_t DISPLAY_WIDTH = 64;
constexpr int16_t DISPLAY_HEIGHT = 16;
constexpr int16_t physicalDisplayX(int16_t x)
{
    return x < 32 ? x + 32 : x - 32;
}
static_assert(physicalDisplayX(0) == 32 && physicalDisplayX(31) == 63 &&
              physicalDisplayX(32) == 0 && physicalDisplayX(63) == 31, "P10 chain order");

// Spec 03 Section 2: inverse of clockwise physical mounting, then chain swap.
constexpr int16_t portraitDmaX(int16_t y) { return physicalDisplayX(y); }
constexpr int16_t portraitDmaY(int16_t x) { return DISPLAY_HEIGHT - 1 - x; }

#endif
