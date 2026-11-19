// Spec 03 Sections 4/5: pure portrait rendering, without a framebuffer.
#ifndef DISPLAY_RENDER_LOGIC_H
#define DISPLAY_RENDER_LOGIC_H

#include <cstdint>
#include "game_state.h"

// Each lit portrait coordinate is emitted once. The caller handles black pixels.
using LitPixelSink = void (*)(void* context, int x, int y);
void renderPortraitPixels(const GameState& state, LitPixelSink sink, void* context);

#endif // DISPLAY_RENDER_LOGIC_H
