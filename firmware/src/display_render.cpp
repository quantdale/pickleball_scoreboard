// Display rendering implementation stub.
// The 64×32 canvas and glyph data are defined in docs/specs/03-display-rendering.md
// and shared/display_assets/; actual drawing is a follow-up change.

#include "display_render.h"

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
    // TODO: draw score digits, server arrows, and center divider per Spec 03 Section 4.
    (void)state;
}
