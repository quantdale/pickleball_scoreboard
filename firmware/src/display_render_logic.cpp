// Spec 03 Sections 4/6: pure 16x64 portrait renderer. Static copies mirror shared JSON;
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
constexpr int CANVAS_WIDTH = 16;
constexpr int CANVAS_HEIGHT = 64;
constexpr int SERVING_TOP_Y = 1;
constexpr int OPPONENT_TOP_Y = 17;
constexpr int SERVER_TOP_Y = 36;
constexpr int DIGIT_SCALE_Y = 2;
constexpr int SINGLE_SCALE_X = 2;
constexpr int DOUBLE_SCALE_X = 1;
constexpr int DIGIT_GAP = 2;
constexpr int ARROW_SCALE = 2;
constexpr int ARROW_TOP_Y = 53;
constexpr int SINGLES_SERVING_TOP_Y = 3;
constexpr int SINGLES_OPPONENT_TOP_Y = 33;
constexpr int SINGLES_ARROW_TOP_Y = 51;
constexpr int DIGIT_HEIGHT = 7 * DIGIT_SCALE_Y;
constexpr int ARROW_HEIGHT = 5 * ARROW_SCALE;
static_assert(SINGLES_SERVING_TOP_Y > 0 && SINGLES_SERVING_TOP_Y + DIGIT_HEIGHT <= 31 &&
              SINGLES_OPPONENT_TOP_Y >= 33 && SINGLES_ARROW_TOP_Y - (SINGLES_OPPONENT_TOP_Y + DIGIT_HEIGHT) >= 4 &&
              SINGLES_SERVING_TOP_Y == CANVAS_HEIGHT - (SINGLES_ARROW_TOP_Y + ARROW_HEIGHT),
              "Singles scores stay on intact panels with a dark seam and equal outer margins");
static_assert(SERVING_TOP_Y >= 1 && OPPONENT_TOP_Y - (SERVING_TOP_Y + DIGIT_HEIGHT) >= 2,
              "Keep top perimeter and separate the scores by at least two dark rows");
static_assert(OPPONENT_TOP_Y + DIGIT_HEIGHT <= 31 && SERVER_TOP_Y >= 33,
              "Keep the physical seam rows 31 and 32 clear");
static_assert(SERVER_TOP_Y + DIGIT_HEIGHT < ARROW_TOP_Y && ARROW_TOP_Y + 5 * ARROW_SCALE < CANVAS_HEIGHT,
              "Separate the arrow from the server and preserve the bottom perimeter");

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

    void number(int value, int top) const
    {
        const bool overflow = value < 0 || value > 99;
        const int count = overflow || value >= 10 ? 2 : 1;
        const int scaleX = count == 1 ? SINGLE_SCALE_X : DOUBLE_SCALE_X;
        const int glyphWidth = 5 * scaleX;
        const int start = (CANVAS_WIDTH - count * glyphWidth - (count - 1) * DIGIT_GAP) / 2;
        for (int index = 0; index < count; ++index) {
            const int x = start + index * (glyphWidth + DIGIT_GAP);
            if (overflow) {
                block(x, top + 3 * DIGIT_SCALE_Y, glyphWidth, DIGIT_SCALE_Y);
                continue;
            }
            const int digit = count == 1 ? value : (index == 0 ? value / 10 : value % 10);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if ((FONT_5X7[digit][row] >> (4 - col)) & 1)
                        block(x + col * scaleX, top + row * DIGIT_SCALE_Y, scaleX, DIGIT_SCALE_Y);
        }
    }
};
} // namespace

void renderPortraitPixels(const GameState& state, LitPixelSink sink, void* context)
{
    const Painter draw{sink, context};
    const bool left = state.servingSide == Side::LEFT;
    // Spec 03 Section 4: singles does not render the BLE server compatibility field.
    const bool singles = state.gameMode == GameMode::SINGLES;
    draw.number(left ? state.leftScore : state.rightScore, singles ? SINGLES_SERVING_TOP_Y : SERVING_TOP_Y);
    draw.number(left ? state.rightScore : state.leftScore, singles ? SINGLES_OPPONENT_TOP_Y : OPPONENT_TOP_Y);
    if (!singles) draw.number(state.serverNumber, SERVER_TOP_Y);
    const int arrowTop = singles ? SINGLES_ARROW_TOP_Y : ARROW_TOP_Y;
    constexpr int arrowX = (CANVAS_WIDTH - 5 * ARROW_SCALE) / 2;
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 5; ++col)
            if ((ARROW_RIGHT[row] >> (left ? col : 4 - col)) & 1)
                draw.block(arrowX + col * ARROW_SCALE, arrowTop + row * ARROW_SCALE,
                           ARROW_SCALE, ARROW_SCALE);
}
