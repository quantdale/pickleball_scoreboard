// Spec 03 Sections 2/5: accepted two-panel driver setup, no scoring logic here.
#include "display_render.h"
#include "display_brightness.h"
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

namespace {
MatrixPanel_I2S_DMA dmaDisplay;
bool initialized = false;
}
static_assert(brightnessValue(BRIGHTNESS_DEFAULT_LEVEL) == P10_BRIGHTNESS, "Spec 03 §2a default");

bool initDisplay(uint8_t brightness)
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
    dmaDisplay.setBrightness8(brightness);
    dmaDisplay.clearScreen();
    Serial.printf("DISPLAY ready 64x16 landscape mount=%u, DMA 64x16, chain=2, brightness=%u/255\n",
                  unsigned(INSTALLED_MOUNT), unsigned(brightness));
    return true;
}

void setDisplayBrightness(uint8_t brightness)
{
    if (!initialized) return;
    dmaDisplay.setBrightness8(brightness);
}

void renderState(const GameState& state)
{
    if (!initialized) return;
    paintScoreboardFrame(dmaDisplay, state);
}
