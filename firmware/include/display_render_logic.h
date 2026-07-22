// Pure-logic display rendering interface.
// Computes which pixels should be lit for a given GameState without touching
// any HUB75 hardware. See docs/specs/03-display-rendering.md.

#ifndef DISPLAY_RENDER_LOGIC_H
#define DISPLAY_RENDER_LOGIC_H

#include <cstdint>
#include <vector>
#include "game_state.h"

// A single lit pixel on the 64×32 logical canvas, with an RGB565 color.
struct RenderPixel {
    int x;
    int y;
    uint16_t color;
};

// Spec 03 Section 5: compute the full set of pixels that should be lit for
// `state`. This function performs no I/O and can be unit tested on the host.
std::vector<RenderPixel> computeRenderedPixels(const GameState& state);

#endif // DISPLAY_RENDER_LOGIC_H
