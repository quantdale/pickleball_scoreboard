#ifndef P10_GEOMETRY_H
#define P10_GEOMETRY_H

#include <stdint.h>

// Bring-up spec: Two-panel horizontal geometry diagnostic.
constexpr int16_t kP10PanelWidth = 32;
constexpr int16_t kP10Height = 16;
constexpr int16_t kP10CanvasWidth = 2 * kP10PanelWidth;

// The first shifted 32-pixel block reaches the far panel; the final block
// remains in the controller-nearest panel. No per-panel mirroring or scan remap.
constexpr int16_t p10RawX(int16_t logicalX)
{
    return logicalX < kP10PanelWidth ? logicalX + kP10PanelWidth : logicalX - kP10PanelWidth;
}
static_assert(p10RawX(0) == 32 && p10RawX(31) == 63, "Panel 1 must use final shifted block");
static_assert(p10RawX(32) == 0 && p10RawX(63) == 31, "Panel 2 must use first shifted block");
constexpr bool p10MappingValid(int16_t x = 0)
{
    return x == kP10CanvasWidth ? true : (p10RawX(x) >= 0 && p10RawX(x) < kP10CanvasWidth &&
           p10RawX(x) % kP10PanelWidth == x % kP10PanelWidth && p10RawX(p10RawX(x)) == x &&
           p10MappingValid(x + 1));
}
static_assert(p10MappingValid(), "All 64 columns must map uniquely in bounds without local mirroring");

#endif // P10_GEOMETRY_H
