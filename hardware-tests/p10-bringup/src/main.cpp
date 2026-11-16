// Isolated HUB75 wiring/colour diagnostic, not the production scoreboard renderer.
// Spec 03 Section 2 describes 32x16 physical modules. The owner's hardware
// bring-up task Section 3 requires RGB/white/black, not production score content.
#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <new>

#ifndef P10_PANEL_COUNT
#error Build with -DP10_PANEL_COUNT=1 or 2
#endif
#if P10_PANEL_COUNT != 1 && P10_PANEL_COUNT != 2
#error P10_PANEL_COUNT must be 1 or 2
#endif
#if P10_PANEL_COUNT == 2
#include "horizontal_canvas.h"
#endif
#ifdef P10_BOUNDARY_TEST
#if P10_PANEL_COUNT != 2
#error The boundary diagnostic requires exactly two panels
#endif
#include "boundary_patterns.h"
#endif

namespace {
constexpr uint16_t kPanelWidth = 32;
constexpr uint16_t kPanelHeight = 16;
constexpr uint8_t kBrightness = 12; // 0..255; low for initial commissioning, NOT a current limiter.
constexpr unsigned long kColorHoldMs = 2000;
#if P10_PANEL_COUNT == 2 && !defined(P10_BOUNDARY_TEST)
constexpr unsigned long kGridHoldMs = 4000;
constexpr unsigned long kColumnHoldMs = 100;
constexpr unsigned long kRowHoldMs = 250;
#endif

// Installed library 3.0.15: HUB75_I2S_CFG::i2s_pins is ordered
// r1,g1,b1,r2,g2,b2,a,b,c,d,e,lat,oe,clk (NOT clk,lat,oe).
// D/E are absent from the physical connector; -1 leaves them unassigned.
constexpr HUB75_I2S_CFG::i2s_pins kPins = {
    25, 26, 27, // R1 G1 B1
    14, 12, 13, // R2 G2 B2
    23, 19, 5,  // A B C
    -1, -1,     // D E (not connected)
    4, 15, 16   // LAT OE CLK
};

struct ColorStep {
    const char* name;
    uint16_t rgb565;
};
constexpr ColorStep kColors[] = {
    {"RED",   0xF800},
    {"GREEN", 0x07E0},
    {"BLUE",  0x001F},
    {"WHITE", 0xFFFF},
    {"BLACK", 0x0000},
};

MatrixPanel_I2S_DMA* display = nullptr;
bool ready = false;
#ifndef P10_BOUNDARY_TEST
size_t nextColor = 0;
#endif
unsigned long lastChange = 0;
#if P10_PANEL_COUNT == 2
// Bring-up spec: Two-panel horizontal geometry diagnostic. Label 1 targets
// the controller-nearest module; physical mounting/orientation remains visual.
HorizontalCanvas canvas;
unsigned long holdMs = kColorHoldMs;
#ifdef P10_BOUNDARY_TEST
uint16_t boundaryFrameIndex = 0;

void runBoundaryFrame()
{
    // Bring-up spec: Pixel-boundary validation profile; every frame clears first.
    const boundary::Frame frame = boundary::frameAt(boundaryFrameIndex);
    boundary::render(canvas, frame);
    Serial.printf("BOUNDARY frame=%u/%u phase=%s coordinate=%d hold_ms=%lu\n",
                  static_cast<unsigned>(boundaryFrameIndex + 1),
                  static_cast<unsigned>(boundary::kFrameCount), boundary::name(frame.pattern),
                  frame.coordinate, static_cast<unsigned long>(frame.holdMs));
    holdMs = frame.holdMs;
    if (++boundaryFrameIndex == boundary::kFrameCount) {
        boundaryFrameIndex = 0;
        Serial.println("BOUNDARY sequence complete; restarting A next");
    }
}
#else
uint8_t dualPhase = 0;
int16_t sweepCoordinate = 0;

void drawHalves(uint16_t left, uint16_t right)
{
    canvas.fillRect(0, 0, kPanelWidth, kPanelHeight, left);
    canvas.fillRect(kPanelWidth, 0, kPanelWidth, kPanelHeight, right);
}

void drawCoordinateGrid()
{
    display->clearScreen();
    // Grid intersections every 8 pixels; axes and seam override grid lines.
    for (int16_t x = 8; x < 64; x += 8)
        canvas.drawFastVLine(x, 0, 16, 0x07FF); // cyan
    canvas.drawFastHLine(0, 8, 64, 0x07FF);
    canvas.drawFastHLine(0, 0, 64, 0xF800); // top red
    canvas.drawFastHLine(0, 15, 64, 0x001F); // bottom blue
    canvas.drawFastVLine(0, 1, 14, 0x07E0); // left green
    canvas.drawFastVLine(63, 1, 14, 0xFFFF); // right white
    canvas.drawFastVLine(31, 1, 14, 0xFFE0); // first half end yellow
    canvas.drawFastVLine(32, 1, 14, 0xF81F); // second half start magenta
    canvas.setTextSize(1);
    canvas.setTextWrap(false);
    canvas.setTextColor(0xFFFF, 0x0000);
    canvas.setCursor(12, 3);
    canvas.print("1");
    canvas.setCursor(44, 3);
    canvas.print("2");
}

void runDualPhase()
{
    switch (dualPhase) {
    case 0:
        if (nextColor < sizeof(kColors) / sizeof(kColors[0])) {
            const ColorStep& color = kColors[nextColor];
            display->fillScreen(color.rgb565);
            Serial.printf("COLOR %s 0x%04X (step %u/5, mode=2)\n", color.name,
                          color.rgb565, static_cast<unsigned>(nextColor + 1));
            ++nextColor;
            if (nextColor == sizeof(kColors) / sizeof(kColors[0])) {
                nextColor = 0;
                dualPhase = 1;
            }
        }
        holdMs = kColorHoldMs;
        break;
    case 1:
        drawHalves(0xF800, 0x001F);
        Serial.println("HALVES P1[0..31]=RED P2[32..63]=BLUE; check physical cable order");
        dualPhase = 2;
        break;
    case 2:
        drawHalves(0x07E0, 0xFFFF);
        Serial.println("HALVES P1[0..31]=GREEN P2[32..63]=WHITE");
        dualPhase = 3;
        break;
    case 3:
        drawCoordinateGrid();
        Serial.println("GRID 64x16: labels 1/2; top=RED bottom=BLUE left=GREEN right=WHITE seam=YELLOW/MAGENTA");
        dualPhase = 4;
        sweepCoordinate = 0;
        holdMs = kGridHoldMs;
        break;
    case 4:
        display->clearScreen();
        canvas.drawFastVLine(sweepCoordinate, 0, 16, 0xFFFF);
        Serial.printf("COLUMN x=%d y=0..15\n", sweepCoordinate);
        holdMs = kColumnHoldMs;
        if (++sweepCoordinate == 64) {
            sweepCoordinate = 0;
            dualPhase = 5;
        }
        break;
    case 5:
        display->clearScreen();
        canvas.drawFastHLine(0, sweepCoordinate, 64, 0xFFFF);
        Serial.printf("ROW y=%d x=0..63\n", sweepCoordinate);
        holdMs = kRowHoldMs;
        if (++sweepCoordinate == 16) {
            sweepCoordinate = 0;
            dualPhase = 0;
            Serial.println("DUAL sequence complete; restarting full-screen colors next");
        }
        break;
    }
}
#endif // P10_BOUNDARY_TEST
#endif // P10_PANEL_COUNT == 2

void fatal(const char* reason)
{
    Serial.printf("ERROR: %s; no color-cycle will run. Disconnect panel power.\n", reason);
    // Do not retry a failed DMA allocation or draw to an uninitialized display.
    ready = false;
}
} // namespace

void setup()
{
    // Active-low OE: blank during software startup, before DMA takes this pin.
    // Reset/bootloader high-impedance time still needs hardware consideration.
    digitalWrite(kPins.oe, HIGH);
    pinMode(kPins.oe, OUTPUT);
    Serial.begin(115200);
    delay(1000);
    Serial.printf("P10 bring-up; mode=%d x 32x16, brightness=%u/255\n",
                  P10_PANEL_COUNT, kBrightness);
    Serial.printf("Chip=%s rev=%u free_heap=%u bytes\n", ESP.getChipModel(),
                  static_cast<unsigned>(ESP.getChipRevision()),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.println("GPIO R1=25 G1=26 B1=27 R2=14 G2=12 B2=13 A=23 B=19 C=5 D=-1 E=-1 LAT=4 OE=15 CLK=16");
    Serial.println("Config: SHIFTREG / TYPE138 / 8 MHz / clkphase=true / normal 1:8 scan.");
    Serial.println("1:8 profile: 8 ABC row addresses, RGB1 rows 0..7, RGB2 rows 8..15; no four-scan remap.");
    Serial.println("Scan type is owner-reported; physical ordering still requires visual geometry verification.");
    Serial.println("Panel detection is not supported: init success does NOT prove a panel is attached.");

    // Library 3.0.15 constructor accepts width, height, chain length, pin map.
    HUB75_I2S_CFG config(kPanelWidth, kPanelHeight, P10_PANEL_COUNT, kPins);
    config.driver = HUB75_I2S_CFG::SHIFTREG;
    config.line_decoder = HUB75_I2S_CFG::TYPE138;
    config.double_buff = false;
    config.i2sspeed = HUB75_I2S_CFG::HZ_8M;
    config.clkphase = true;

    display = new (std::nothrow) MatrixPanel_I2S_DMA(config);
    if (!display) {
        fatal("MatrixPanel allocation failed");
        return;
    }
    Serial.println("Initializing HUB75 DMA...");
    if (!display->begin()) {
        fatal("HUB75 DMA begin() failed (check internal DMA RAM/driver logs)");
        return;
    }
    // setBrightness8 only works AFTER begin(); repaint after changing OE duty.
    display->setBrightness8(kBrightness);
    display->clearScreen(); // controlled black state before any other color
    if (display->width() != kPanelWidth * P10_PANEL_COUNT || display->height() != kPanelHeight) {
        fatal("Unexpected logical canvas dimensions");
        return;
    }
#if P10_PANEL_COUNT == 2
    canvas.attach(*display);
    Serial.println("MAP logical P1 x=0..31 -> raw x=32..63 (near); P2 x=32..63 -> raw x=0..31 (far)");
#ifdef P10_BOUNDARY_TEST
    Serial.println("BOUNDARY v1: A/B -> corners/seam/isolation -> green-white -> checkerboards -> columns -> rows");
    Serial.println("BOUNDARY: 5-second statics; clear before EVERY frame; no timing/scan changes; physical green strip unresolved");
#else
    Serial.println("DUAL: fills -> independent halves -> grid -> columns 0..63 -> rows 0..15; repeat");
#endif
#endif
    ready = true;
    Serial.printf("HUB75 DMA initialized; logical raw size=%d x %d; BLACK at startup.\n",
                  display->width(), display->height());
    Serial.println("No panel presence or electrical safety check is possible in software.");
    lastChange = millis();
}

void loop()
{
    if (!ready) {
        delay(1000);
        return;
    }
#if P10_PANEL_COUNT == 2
    if (millis() - lastChange < holdMs) {
        delay(1);
        return;
    }
#ifdef P10_BOUNDARY_TEST
    runBoundaryFrame();
#else
    runDualPhase();
#endif
#else
    if (millis() - lastChange < kColorHoldMs) {
        delay(10);
        return;
    }
    const ColorStep& color = kColors[nextColor];
    display->fillScreen(color.rgb565);
    Serial.printf("COLOR %s 0x%04X (step %u/5, mode=%d)\n",
                  color.name, color.rgb565, static_cast<unsigned>(nextColor + 1), P10_PANEL_COUNT);
    nextColor = (nextColor + 1) % (sizeof(kColors) / sizeof(kColors[0]));
#endif
    lastChange = millis();
}
