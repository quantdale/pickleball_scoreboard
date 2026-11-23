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

// Spec 03 Section 2: supported landscape mounts. Raw x32..63 is Panel 1, the
// panel wired to the ESP32; the verified portrait witness had it on top.
enum class LandscapeMount : uint8_t {
    PortraitTurnedClockwise = 1,        // Panel 1 on the right; identity
    PortraitTurnedCounterClockwise = 2, // Panel 1 on the left, both axes reversed
    BringUpOrder = 3,                   // Panel 1 on the left, axes preserved
};

#ifndef PICKLE_LANDSCAPE_MOUNT
#define PICKLE_LANDSCAPE_MOUNT 1
#endif
static_assert(PICKLE_LANDSCAPE_MOUNT >= 1 && PICKLE_LANDSCAPE_MOUNT <= 3, "Unknown landscape mount");
constexpr LandscapeMount INSTALLED_MOUNT = static_cast<LandscapeMount>(PICKLE_LANDSCAPE_MOUNT);

constexpr int16_t landscapeDmaX(int16_t x, LandscapeMount mount = INSTALLED_MOUNT)
{
    return mount == LandscapeMount::PortraitTurnedCounterClockwise ? DISPLAY_WIDTH - 1 - x
         : mount == LandscapeMount::BringUpOrder ? physicalDisplayX(x) : x;
}
constexpr int16_t landscapeDmaY(int16_t y, LandscapeMount mount = INSTALLED_MOUNT)
{
    return mount == LandscapeMount::PortraitTurnedCounterClockwise ? DISPLAY_HEIGHT - 1 - y : y;
}
static_assert(landscapeDmaX(31, LandscapeMount::PortraitTurnedClockwise) == 31 &&
              landscapeDmaX(32, LandscapeMount::PortraitTurnedClockwise) == 32, "Landscape panel seam");

#endif
