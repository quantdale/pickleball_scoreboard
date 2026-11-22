// Spare-only P10 diagnostic. Spec 03 §2: use the installed display settings.
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <soc/i2s_struct.h>
#include "color_isolation.h"

namespace {
constexpr HUB75_I2S_CFG::i2s_pins pins = {25,26,27,14,12,13,23,19,5,-1,-1,4,15,16};
MatrixPanel_I2S_DMA display;
colorIsolation::Frame frame;
bool ready = false;

void paint()
{
    display.clearScreen();
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 64; ++x) {
            const uint16_t color = frame.pixel(x, y);
            if (color) display.drawPixel(x, y, color);
        }
    }
    Serial.printf("P10 held: phase=%d color=%c block=%c\n", frame.phase, frame.color, frame.block);
}
}

void setup()
{
    digitalWrite(pins.oe, HIGH); // Active-low OE; not a reset/bootloader guarantee.
    pinMode(pins.oe, OUTPUT);
    Serial.begin(115200);
    Serial.println("P10 COLOR ISOLATION: spare only; no BLE/scoring/OTA");
    HUB75_I2S_CFG config(32, 16, 2, pins);
    config.driver = HUB75_I2S_CFG::SHIFTREG;
    config.line_decoder = HUB75_I2S_CFG::TYPE138;
    config.double_buff = false;
    config.i2sspeed = HUB75_I2S_CFG::HZ_8M;
    config.clkphase = true;
    if (!display.begin(config)) {
        Serial.println("ERROR: DMA init failed; disconnect panel power");
        return;
    }
    // The pinned original-ESP32 backend uses I2S1; this is register readback,
    // not a frequency or logic-level measurement at the receiver.
    const auto& i2s = ESP32_I2S_DEVICE == I2S_NUM_0 ? I2S0 : I2S1;
    const uint32_t mclk = i2s.clkm_conf.val;
    const uint32_t rate = i2s.sample_rate_conf.val;
    Serial.printf("HUB75 requested=8000000 phase=1 blank=%u I2S%d CLKM_CONF=0x%08lx SAMPLE_RATE_CONF=0x%08lx\n",
                  static_cast<unsigned>(config.latch_blanking), static_cast<int>(ESP32_I2S_DEVICE),
                  static_cast<unsigned long>(mclk), static_cast<unsigned long>(rate));
    display.setBrightness8(12);
    paint();
    ready = true;
    Serial.println("0..7 phase; w/r/g/b color; k black; a both/n near/f far. Wait for steady frame before capture.");
}

void loop()
{
    if (ready && frame.select(Serial.read())) paint();
    delay(1);
}
