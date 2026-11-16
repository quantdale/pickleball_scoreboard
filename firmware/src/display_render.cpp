// Spec 03 Sections 2/5: accepted two-panel driver setup, no scoring logic here.
#include "display_render.h"
#include "display_render_logic.h"
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace {
MatrixPanel_I2S_DMA dmaDisplay;
bool initialized = false;
}

bool initDisplay()
{
    digitalWrite(15, HIGH); // active-low OE blank during setup, not a bootloader guarantee
    pinMode(15, OUTPUT);
    const HUB75_I2S_CFG::i2s_pins pins = {25,26,27,14,12,13,23,19,5,-1,-1,4,15,16};
    HUB75_I2S_CFG config(32, 16, 2, pins);
    config.driver = HUB75_I2S_CFG::SHIFTREG;
    config.line_decoder = HUB75_I2S_CFG::TYPE138;
    config.double_buff = false;
    config.i2sspeed = HUB75_I2S_CFG::HZ_8M;
    config.clkphase = true;
    initialized = dmaDisplay.begin(config);
    if (!initialized) {
        Serial.println("ERROR: HUB75 DMA initialization failed; display disabled");
        return false;
    }
    dmaDisplay.setBrightness8(P10_BRIGHTNESS);
    dmaDisplay.clearScreen();
    Serial.println("DISPLAY ready 64x16, chain=2, brightness=12/255");
    return true;
}

void renderState(const GameState& state)
{
    if (!initialized) return;
    const auto pixels = computeRenderedPixels(state);
    dmaDisplay.clearScreen();
    for (const auto& pixel : pixels)
        dmaDisplay.drawPixel(physicalDisplayX(static_cast<int16_t>(pixel.x)), pixel.y, pixel.color);
}
