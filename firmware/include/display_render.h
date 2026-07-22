// Display rendering interface.
// See docs/specs/03-display-rendering.md for the authoritative canvas/layout spec.

#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "game_state.h"

// Spec 03 Section 2: 64×32 logical canvas.
constexpr int DISPLAY_WIDTH = 64;
constexpr int DISPLAY_HEIGHT = 32;

// Global HUB75 panel instance; initialized by initDisplay().
extern MatrixPanel_I2S_DMA dma_display;

// Initialize the LED panel via ESP32-HUB75-MatrixPanel-I2S-DMA.
void initDisplay();

// Render the current game state to the 64×32 canvas (drawing logic is TODO).
void renderState(const GameState& state);

#endif // DISPLAY_RENDER_H
