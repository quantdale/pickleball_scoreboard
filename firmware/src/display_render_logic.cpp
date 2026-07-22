// Pure-logic display rendering implementation.
// Computes the lit pixels for any GameState per docs/specs/03-display-rendering.md.
//
// DEVIATION from Spec 03 Section 6: the firmware does NOT parse the shared
// JSON files at runtime. The glyph bit patterns below are hand-transcribed
// copies of shared/display_assets/font_5x7.json and arrows.json. The JSON
// files remain the design-time source of truth; these C++ arrays are a
// separate compiled copy that must be kept in sync with the JSON.

#include "display_render_logic.h"

namespace {

// Spec 03 Section 4a: 5×7 digit font. Each row is stored as the lower 5 bits
// of a byte (bit 4 = left-most pixel). Index 0 = '0', index 9 = '9'.
// Source: shared/display_assets/font_5x7.json
constexpr uint8_t FONT_5X7[10][7] = {
    { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 }, // 0
    { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 }, // 1
    { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 }, // 2
    { 0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110 }, // 3
    { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 }, // 4
    { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 }, // 5
    { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 }, // 6
    { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 }, // 7
    { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 }, // 8
    { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 }  // 9
};

constexpr int DIGIT_WIDTH = 5;
constexpr int DIGIT_HEIGHT = 7;
constexpr int DIGIT_GAP = 1;

// Spec 03 Section 4b: 9×7 server arrow bitmap. Each row is stored as a
// uint16_t with the lower 9 bits (bit 8 = left-most pixel).
// Source: shared/display_assets/arrows.json (ARROW_RIGHT only; ARROW_LEFT is
// derived at runtime by reversing each row, per Spec 03 Section 4b).
constexpr uint16_t ARROW_RIGHT[7] = {
    0b000001000,
    0b000001100,
    0b111111110,
    0b111111111,
    0b111111110,
    0b000001100,
    0b000001000
};

constexpr int ARROW_WIDTH = 9;
constexpr int ARROW_HEIGHT = 7;
constexpr int ARROW_GAP = 1;

// Spec 03 Section 2.
constexpr int CANVAS_WIDTH = 64;
constexpr int CANVAS_HEIGHT = 32;

// Spec 03 Section 4: horizontal centers of the left/right halves.
constexpr float LEFT_CENTER_X = 15.5f;
constexpr float RIGHT_CENTER_X = 47.5f;

// Vertical positions chosen to match the mockup's 8% arrow top and centered
// digit block below (Spec 03 Section 4).
constexpr int ARROW_TOP_Y = 3;
constexpr int DIGIT_TOP_Y = 15;

// Spec 03 Section 3: lit pixels are white (#FFFFFF → RGB565 0xFFFF).
constexpr uint16_t WHITE_RGB565 = 0xFFFF;

int countDigits(int value) {
    if (value == 0) return 1;
    int digits = 0;
    while (value > 0) {
        value /= 10;
        ++digits;
    }
    return digits;
}

int getDigit(int value, int index, int totalDigits) {
    int divisor = 1;
    for (int i = 0; i < totalDigits - 1 - index; ++i) {
        divisor *= 10;
    }
    return (value / divisor) % 10;
}

void addPixel(std::vector<RenderPixel>& pixels, int x, int y, uint16_t color) {
    if (x < 0 || x >= CANVAS_WIDTH || y < 0 || y >= CANVAS_HEIGHT) return;
    pixels.push_back({x, y, color});
}

void addDigit(std::vector<RenderPixel>& pixels, int digit, int x, int y, uint16_t color) {
    if (digit < 0 || digit > 9) return;
    for (int row = 0; row < DIGIT_HEIGHT; ++row) {
        uint8_t rowBits = FONT_5X7[digit][row];
        for (int col = 0; col < DIGIT_WIDTH; ++col) {
            if ((rowBits >> (DIGIT_WIDTH - 1 - col)) & 1) {
                addPixel(pixels, x + col, y + row, color);
            }
        }
    }
}

void addNumber(std::vector<RenderPixel>& pixels, int number, float centerX, int y, uint16_t color) {
    if (number < 0) number = 0;
    const int digits = countDigits(number);
    const int totalWidth = digits * DIGIT_WIDTH + (digits - 1) * DIGIT_GAP;
    const int startX = static_cast<int>(centerX - totalWidth / 2.0f + 0.5f);
    for (int i = 0; i < digits; ++i) {
        const int digit = getDigit(number, i, digits);
        const int x = startX + i * (DIGIT_WIDTH + DIGIT_GAP);
        addDigit(pixels, digit, x, y, color);
    }
}

uint16_t reverseArrowRow(uint16_t row) {
    uint16_t reversed = 0;
    for (int i = 0; i < ARROW_WIDTH; ++i) {
        reversed = (reversed << 1) | ((row >> i) & 1);
    }
    return reversed;
}

void addArrow(std::vector<RenderPixel>& pixels, Side side, int count, float centerX, int y, uint16_t color) {
    if (count < 1) return;
    const int totalWidth = count * ARROW_WIDTH + (count - 1) * ARROW_GAP;
    const int startX = static_cast<int>(centerX - totalWidth / 2.0f + 0.5f);
    for (int arrowIndex = 0; arrowIndex < count; ++arrowIndex) {
        const int x = startX + arrowIndex * (ARROW_WIDTH + ARROW_GAP);
        for (int row = 0; row < ARROW_HEIGHT; ++row) {
            uint16_t rowBits = ARROW_RIGHT[row];
            if (side == Side::LEFT) {
                rowBits = reverseArrowRow(rowBits);
            }
            for (int col = 0; col < ARROW_WIDTH; ++col) {
                if ((rowBits >> (ARROW_WIDTH - 1 - col)) & 1) {
                    addPixel(pixels, x + col, y + row, color);
                }
            }
        }
    }
}

// Spec 03 Section 4c: thin center divider rendered as a sparse dotted line so
// it reads as "dim" on a panel that only supports full-on/full-off pixels.
void addCenterDivider(std::vector<RenderPixel>& pixels, uint16_t color) {
    constexpr int dividerX = CANVAS_WIDTH / 2;
    for (int y = 4; y <= 28; y += 4) {
        addPixel(pixels, dividerX, y, color);
    }
}

} // namespace

std::vector<RenderPixel> computeRenderedPixels(const GameState& state) {
    std::vector<RenderPixel> pixels;
    pixels.reserve(256); // rough reserve for typical scoreboard content

    addCenterDivider(pixels, WHITE_RGB565);
    addNumber(pixels, state.leftScore, LEFT_CENTER_X, DIGIT_TOP_Y, WHITE_RGB565);
    addNumber(pixels, state.rightScore, RIGHT_CENTER_X, DIGIT_TOP_Y, WHITE_RGB565);

    if (state.servingSide == Side::LEFT) {
        addArrow(pixels, Side::LEFT, state.serverNumber, LEFT_CENTER_X, ARROW_TOP_Y, WHITE_RGB565);
    } else {
        addArrow(pixels, Side::RIGHT, state.serverNumber, RIGHT_CENTER_X, ARROW_TOP_Y, WHITE_RGB565);
    }

    return pixels;
}
