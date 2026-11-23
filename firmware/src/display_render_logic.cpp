// Spec 03 Sections 4/6: pure 64x16 landscape renderer. Static copies mirror shared JSON;
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
constexpr uint8_t ARROW_RIGHT[12] = {
    0b100000,
    0b110000,
    0b111000,
    0b111100,
    0b111110,
    0b111111,
    0b111111,
    0b111110,
    0b111100,
    0b111000,
    0b110000,
    0b100000
};
constexpr int CANVAS_WIDTH = 64;
constexpr int CANVAS_HEIGHT = 16;
constexpr int DIGIT_TOP_Y = 1;
constexpr int DIGIT_SCALE_X = 2;
constexpr int DIGIT_SCALE_Y = 2;
constexpr int DIGIT_GAP = 2;
constexpr int LEFT_SCORE_X = 1;
constexpr int RIGHT_SCORE_X = 41;
constexpr int SCORE_WIDTH = 22;
constexpr int ARROW_TOP_Y = 2;
constexpr int ARROW_GAP = 2;
constexpr int ARROW_WIDTH = 6;
constexpr int ARROW_HEIGHT = 12;
constexpr int DIGIT_WIDTH = 5 * DIGIT_SCALE_X;
constexpr int PAIR_WIDTH = 2 * ARROW_WIDTH + ARROW_GAP;
constexpr int PAIR_X = (CANVAS_WIDTH - PAIR_WIDTH) / 2;
static_assert(2 * DIGIT_WIDTH + DIGIT_GAP == SCORE_WIDTH, "Two digits fill each score region");
static_assert(LEFT_SCORE_X == 1 && RIGHT_SCORE_X + SCORE_WIDTH == CANVAS_WIDTH - 1,
              "Scores stay inside the dark outer columns, mirrored about the center");
static_assert(PAIR_X - (LEFT_SCORE_X + SCORE_WIDTH) >= 2 && RIGHT_SCORE_X - (PAIR_X + PAIR_WIDTH) >= 2,
              "Keep at least two dark columns between each score and the arrows");
static_assert(PAIR_X + ARROW_WIDTH <= CANVAS_WIDTH / 2 - 1 && PAIR_X + ARROW_WIDTH + ARROW_GAP >= CANVAS_WIDTH / 2 + 1,
              "Two arrows leave the panel seam (x31|x32) dark");
static_assert(DIGIT_TOP_Y >= 1 && DIGIT_TOP_Y + 7 * DIGIT_SCALE_Y <= CANVAS_HEIGHT - 1 &&
              ARROW_TOP_Y >= 1 && ARROW_TOP_Y + ARROW_HEIGHT <= CANVAS_HEIGHT - 1,
              "Keep the top and bottom rows dark");

struct Painter {
    LitPixelSink sink;
    void* context;

    void block(int x, int y, int width, int height) const
    {
        for (int dy = 0; dy < height; ++dy)
            for (int dx = 0; dx < width; ++dx) {
                const int px = x + dx;
                const int py = y + dy;
                if (px > 0 && px < CANVAS_WIDTH - 1 && py > 0 && py < CANVAS_HEIGHT - 1)
                    sink(context, px, py);
            }
    }

    // Spec 03 §4a: one centered digit or two digits filling the 22-column region.
    void number(int value, int regionX) const
    {
        const bool overflow = value < 0 || value > 99;
        const int count = overflow || value >= 10 ? 2 : 1;
        const int start = regionX + (SCORE_WIDTH - count * DIGIT_WIDTH - (count - 1) * DIGIT_GAP) / 2;
        for (int index = 0; index < count; ++index) {
            const int x = start + index * (DIGIT_WIDTH + DIGIT_GAP);
            if (overflow) {
                block(x, DIGIT_TOP_Y + 3 * DIGIT_SCALE_Y, DIGIT_WIDTH, DIGIT_SCALE_Y);
                continue;
            }
            const int digit = count == 1 ? value : (index == 0 ? value / 10 : value % 10);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if ((FONT_5X7[digit][row] >> (4 - col)) & 1)
                        block(x + col * DIGIT_SCALE_X, DIGIT_TOP_Y + row * DIGIT_SCALE_Y, DIGIT_SCALE_X, DIGIT_SCALE_Y);
        }
    }

    // Spec 03 §4b: the source points right; the left team's serve mirrors it.
    void arrow(int x, bool pointLeft) const
    {
        for (int row = 0; row < ARROW_HEIGHT; ++row)
            for (int col = 0; col < ARROW_WIDTH; ++col)
                if ((ARROW_RIGHT[row] >> (pointLeft ? col : ARROW_WIDTH - 1 - col)) & 1)
                    block(x + col, ARROW_TOP_Y + row, 1, 1);
    }
};
} // namespace

void renderLandscapePixels(const GamePosition& state, LitPixelSink sink, void* context)
{
    const Painter draw{sink, context};
    draw.number(state.leftScore, LEFT_SCORE_X);
    draw.number(state.rightScore, RIGHT_SCORE_X);
    const bool pointLeft = state.servingSide == Side::LEFT;
    // Spec 03 §4b: Singles always shows one arrow.
    if (state.gameMode == GameMode::DOUBLES && state.serverNumber == 2) {
        draw.arrow(PAIR_X, pointLeft);
        draw.arrow(PAIR_X + ARROW_WIDTH + ARROW_GAP, pointLeft);
    } else {
        draw.arrow((CANVAS_WIDTH - ARROW_WIDTH) / 2, pointLeft);
    }
}
