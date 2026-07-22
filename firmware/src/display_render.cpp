// Display rendering implementation.
// See docs/specs/03-display-rendering.md for the authoritative canvas/layout spec.
// Hardware-specific drawing calls live here; pixel computation is in
// display_render_logic.h/.cpp so it can be unit tested without HUB75 hardware.

#include "display_render.h"
#include "display_render_logic.h"

MatrixPanel_I2S_DMA dma_display;

void initDisplay() {
    // Configure the HUB75 driver for a 2×2 grid of 32×16 panels (64×32 total).
    HUB75_I2S_CFG mxconfig;
    mxconfig.mx_width = 32;
    mxconfig.mx_height = 16;
    mxconfig.chain_length = 4;
    // Panel arrangement from Spec 03 Section 2: top-left, top-right, bottom-left, bottom-right.

    dma_display.begin(mxconfig);
    dma_display.clearScreen();
}

void renderState(const GameState& state) {
    // Spec 03 Section 5: full redraw on every state change.
    dma_display.clearScreen();

    const auto pixels = computeRenderedPixels(state);
    for (const auto& p : pixels) {
        dma_display.drawPixel(p.x, p.y, p.color);
    }
}
