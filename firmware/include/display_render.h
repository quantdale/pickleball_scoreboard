#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H

#include "display_render_logic.h"
#include "p10_hardware.h"

// Spec 03 Sections 2/5: safe hardware initialization, draw only after success.
bool initDisplay();
void renderState(const GameState& state);

// Spec 03 §§2/3/5: the driver adapter only requests black and full RGB white.
// The template lets host tests observe the same clear and raw-coordinate writes.
template <typename DmaDisplay>
void paintScoreboardFrame(DmaDisplay& display, const GameState& state)
{
    display.clearScreen();
    renderLandscapePixels(state, [](void* context, int x, int y) {
        static_cast<DmaDisplay*>(context)->drawPixel(landscapeDmaX(x), landscapeDmaY(y), uint16_t{0xFFFF});
    }, &display);
}

#endif
