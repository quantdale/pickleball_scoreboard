// Spec 03 Sections 4/5: pure landscape rendering, without a framebuffer.
#ifndef DISPLAY_RENDER_LOGIC_H
#define DISPLAY_RENDER_LOGIC_H

#include <cstdint>
#include "game_state.h"

// Each lit 64x16 coordinate is emitted once. The caller handles black pixels.
using LitPixelSink = void (*)(void* context, int x, int y);
void renderLandscapePixels(const GamePosition& state, LitPixelSink sink, void* context);

#endif // DISPLAY_RENDER_LOGIC_H
