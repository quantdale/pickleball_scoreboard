// Spec 03 Sections 4/6: pure 64x16 renderer. Static copies mirror shared JSON;
// scripts/check_glyphs.py verifies glyph/layout drift. No runtime JSON on ESP32.
// Only lit pixels are emitted; the hardware adapter clears once per state change.
#include "display_render_logic.h"

namespace {
constexpr uint8_t FONT_5X7[10][7] = {
    { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 },
    { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 },
    { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 },
    { 0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110 },
    { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 },
    { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 },
    { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 },
    { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 },
    { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 },
    { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 }
};
constexpr uint16_t ARROW_RIGHT[5] = {
    0b00100,
    0b00010,
    0b11111,
    0b00010,
    0b00100
};
constexpr int CANVAS_WIDTH = 64;
constexpr int CANVAS_HEIGHT = 16;
constexpr float LEFT_CENTER_X = 18.5f;
constexpr float RIGHT_CENTER_X = 44.5f;
constexpr int DIGIT_TOP_Y = 1;
constexpr int DIGIT_SCALE = 2;
constexpr int DIGIT_GAP = 2;
constexpr int LEFT_ARROW_X = 1;
constexpr int RIGHT_ARROW_X = 58;
constexpr int ARROW_SINGLE_TOP_Y = 5;
constexpr int ARROW_DOUBLE_TOP_Y = 2;
constexpr int ARROW_GAP = 2;
constexpr int ARROW_WIDTH = 5;
constexpr int ARROW_HEIGHT = 5;

void addPixel(std::vector<RenderPixel>& pixels, int x, int y)
{
    if (x >= 0 && x < CANVAS_WIDTH && y >= 0 && y < CANVAS_HEIGHT)
        pixels.push_back({x, y, 0xFFFF});
}

void addNumber(std::vector<RenderPixel>& pixels, int number, float center)
{
    const bool overflow = number < 0 || number > 99;
    const int count = overflow || number >= 10 ? 2 : 1;
    const int width = count * 5 * DIGIT_SCALE + (count - 1) * DIGIT_GAP;
    const int start = static_cast<int>(center - width / 2.0f + 0.5f);
    for (int digitIndex = 0; digitIndex < count; ++digitIndex) {
        const int x = start + digitIndex * (5 * DIGIT_SCALE + DIGIT_GAP);
        if (overflow) {
            // Spec 03 Section 4a: visible overflow, never wrap/cap the game score.
            for (int col = 0; col < 10; ++col)
                for (int row = 7; row <= 8; ++row) addPixel(pixels, x + col, row);
            continue;
        }
        const int digit = count == 1 ? number : (digitIndex == 0 ? number / 10 : number % 10);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if ((FONT_5X7[digit][row] >> (4 - col)) & 1)
                    for (int dy = 0; dy < DIGIT_SCALE; ++dy)
                        for (int dx = 0; dx < DIGIT_SCALE; ++dx)
                            addPixel(pixels, x + col * DIGIT_SCALE + dx,
                                     DIGIT_TOP_Y + row * DIGIT_SCALE + dy);
    }
}
} // namespace

std::vector<RenderPixel> computeRenderedPixels(const GameState& state)
{
    std::vector<RenderPixel> pixels;
    pixels.reserve(512); // lit content only, once per state change, not a framebuffer
    addNumber(pixels, state.leftScore, LEFT_CENTER_X);
    addNumber(pixels, state.rightScore, RIGHT_CENTER_X);
    const int count = state.serverNumber == 1 ? 1 : 2;
    const int x = state.servingSide == Side::LEFT ? LEFT_ARROW_X : RIGHT_ARROW_X;
    const int top = count == 1 ? ARROW_SINGLE_TOP_Y : ARROW_DOUBLE_TOP_Y;
    for (int arrow = 0; arrow < count; ++arrow)
        for (int row = 0; row < ARROW_HEIGHT; ++row)
            for (int col = 0; col < ARROW_WIDTH; ++col) {
                const int bit = state.servingSide == Side::LEFT ? col : ARROW_WIDTH - 1 - col;
                if ((ARROW_RIGHT[row] >> bit) & 1)
                    addPixel(pixels, x + col, top + arrow * (ARROW_HEIGHT + ARROW_GAP) + row);
            }
    return pixels;
}
