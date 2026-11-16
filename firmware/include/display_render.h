#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H

#include "game_state.h"
#include "p10_hardware.h"

// Spec 03 Sections 2/5: safe hardware initialization, draw only after success.
bool initDisplay();
void renderState(const GameState& state);

#endif
