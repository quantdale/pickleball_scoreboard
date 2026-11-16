#ifndef P10_HORIZONTAL_CANVAS_H
#define P10_HORIZONTAL_CANVAS_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "p10_geometry.h"

// Bring-up spec: Two-panel horizontal geometry diagnostic.
// The raw DMA stream shifts x=0..31 first, through the near panel into the far
// panel. The final 32 samples remain in the controller-nearest panel (Panel 1).
// Present Panel 1 on logical left and Panel 2 on logical right, preserving each
// module's local coordinates. This is chain-block ordering, NOT four-scan mapping.
// Actual panel orientation/order must still be confirmed from the visible labels.
class HorizontalCanvas : public Adafruit_GFX {
public:
    HorizontalCanvas() : Adafruit_GFX(64, 16) {}

    void attach(MatrixPanel_I2S_DMA& display) { dma = &display; }

    void clearScreen() { if (dma) dma->clearScreen(); }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (dma && x >= 0 && x < 64 && y >= 0 && y < 16)
            dma->drawPixel(p10RawX(x), y, color);
    }

private:
    MatrixPanel_I2S_DMA* dma = nullptr;
};

#endif // P10_HORIZONTAL_CANVAS_H
