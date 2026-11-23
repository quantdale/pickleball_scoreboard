#ifndef DISPLAY_RENDER_H
#define DISPLAY_RENDER_H

#include "display_render_logic.h"
#include "p10_hardware.h"

// Spec 03 Sections 2/5: safe hardware initialization, draw only after success.
bool initDisplay(uint8_t brightness, uint16_t color565);
void renderState(const GameState& state);
// Spec 03 §2a: rewrites OE timing in the existing DMA buffer; pixels are untouched.
void setDisplayBrightness(uint8_t brightness);
// Spec 03 §3: used from the next renderState; the caller repaints.
void setDisplayColor(uint16_t color565);

// Spec 03 §§2/3/5: the driver adapter only requests black and one full-channel
// palette color (display_color.h) for every lit pixel.
// The template lets host tests observe the same clear and raw-coordinate writes.
template <typename DmaDisplay>
struct PixelSink {
    DmaDisplay* display;
    uint16_t color;
};
template <typename DmaDisplay>
void paintScoreboardFrame(DmaDisplay& display, const GameState& state, uint16_t color565)
{
    display.clearScreen();
    PixelSink<DmaDisplay> sink{&display, color565};
    renderLandscapePixels(state, [](void* context, int x, int y) {
        const auto* target = static_cast<PixelSink<DmaDisplay>*>(context);
        target->display->drawPixel(landscapeDmaX(x), landscapeDmaY(y), target->color);
    }, &sink);
}

#endif
