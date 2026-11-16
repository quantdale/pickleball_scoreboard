#ifndef P10_BOUNDARY_PATTERNS_H
#define P10_BOUNDARY_PATTERNS_H

#include "p10_geometry.h"

// Bring-up spec: Pixel-boundary validation profile. Pure pixel calculations are
// shared by firmware and host tests. Ranges are explicit: [0,32) and [32,64).
namespace boundary {
constexpr uint16_t BLACK = 0x0000;
constexpr uint16_t RED = 0xF800;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t BLUE = 0x001F;
constexpr uint16_t WHITE = 0xFFFF;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t PURPLE = 0xF81F; // full red + blue (magenta/purple)
constexpr uint16_t ORANGE = 0xFC00;

enum class Pattern : uint8_t {
    A, B, CORNERS, X31, X32, SEAM, PANEL1_ONLY, PANEL2_ONLY,
    GREEN_WHITE, CHECKER, CHECKER_INVERSE, COLUMN, ROW
};
struct Frame {
    Pattern pattern;
    int16_t coordinate;
    uint32_t holdMs;
};
constexpr uint16_t kStaticFrames = 11;
constexpr uint16_t kFrameCount = kStaticFrames + kP10CanvasWidth + kP10Height;

inline Frame frameAt(uint16_t index)
{
    static const Pattern statics[kStaticFrames] = {
        Pattern::A, Pattern::B, Pattern::CORNERS, Pattern::X31, Pattern::X32,
        Pattern::SEAM, Pattern::PANEL1_ONLY, Pattern::PANEL2_ONLY,
        Pattern::GREEN_WHITE, Pattern::CHECKER, Pattern::CHECKER_INVERSE
    };
    if (index < kStaticFrames) return {statics[index], -1, 5000};
    if (index < kStaticFrames + kP10CanvasWidth)
        return {Pattern::COLUMN, static_cast<int16_t>(index - kStaticFrames), 100};
    if (index < kFrameCount)
        return {Pattern::ROW, static_cast<int16_t>(index - kStaticFrames - kP10CanvasWidth), 250};
    return {Pattern::A, -1, 5000}; // defensive fallback; sequencer wraps explicitly
}

inline const char* name(Pattern pattern)
{
    static const char* const names[] = {
        "A red/blue | yellow/cyan borders/fills",
        "B green/purple | red/yellow borders/fills",
        "C corners and edge lines x=0,31,32,63",
        "C x=31 WHITE only; Panel2 BLACK",
        "C x=32 WHITE only; Panel1 BLACK",
        "C seam x=31 RED x=32 GREEN; rest BLACK",
        "C Panel1 GREEN only; Panel2 BLACK",
        "C Panel2 WHITE only; Panel1 BLACK",
        "REPRO Panel1 GREEN | Panel2 WHITE",
        "D checker even(x+y)=WHITE odd=BLACK",
        "D checker even(x+y)=BLACK odd=WHITE",
        "D COLUMN WHITE",
        "D ROW WHITE"
    };
    const uint8_t index = static_cast<uint8_t>(pattern);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "INVALID";
}

inline uint16_t colorAt(const Frame& frame, int16_t x, int16_t y)
{
    if (x < 0 || x >= kP10CanvasWidth || y < 0 || y >= kP10Height) return BLACK;
    const int16_t panel = x / kP10PanelWidth;
    const int16_t localX = x - panel * kP10PanelWidth;
    const bool edge = localX == 0 || localX == kP10PanelWidth - 1 || y == 0 || y == kP10Height - 1;
    switch (frame.pattern) {
    case (Pattern::A):
        return panel == 0 ? (edge ? RED : BLUE) : (edge ? YELLOW : CYAN);
    case (Pattern::B):
        return panel == 0 ? (edge ? GREEN : PURPLE) : (edge ? RED : YELLOW);
    case (Pattern::CORNERS):
        // Eight distinct corner colors; vertical edge lines occupy y=1..14.
        if (localX == 0 && y == 0) return panel == 0 ? RED : CYAN;
        if (localX == 31 && y == 0) return panel == 0 ? GREEN : PURPLE;
        if (localX == 0 && y == 15) return panel == 0 ? BLUE : YELLOW;
        if (localX == 31 && y == 15) return panel == 0 ? WHITE : ORANGE;
        if (x == 0) return RED;
        if (x == 31) return GREEN;
        if (x == 32) return YELLOW;
        if (x == 63) return BLUE;
        return BLACK;
    case (Pattern::X31): return x == 31 ? WHITE : BLACK;
    case (Pattern::X32): return x == 32 ? WHITE : BLACK;
    case (Pattern::SEAM): return x == 31 ? RED : (x == 32 ? GREEN : BLACK);
    case (Pattern::PANEL1_ONLY): return panel == 0 ? GREEN : BLACK;
    case (Pattern::PANEL2_ONLY): return panel == 1 ? WHITE : BLACK;
    case (Pattern::GREEN_WHITE): return panel == 0 ? GREEN : WHITE;
    case (Pattern::CHECKER): return ((x + y) & 1) == 0 ? WHITE : BLACK;
    case (Pattern::CHECKER_INVERSE): return ((x + y) & 1) == 0 ? BLACK : WHITE;
    case (Pattern::COLUMN): return x == frame.coordinate ? WHITE : BLACK;
    case (Pattern::ROW): return y == frame.coordinate ? WHITE : BLACK;
    }
    return BLACK;
}

template<typename Canvas>
void render(Canvas& canvas, const Frame& frame)
{
    canvas.clearScreen(); // every static and sweep frame starts entirely black
    for (int16_t y = 0; y < kP10Height; ++y) {
        for (int16_t x = 0; x < kP10CanvasWidth; ++x) {
            const uint16_t color = colorAt(frame, x, y);
            if (color != BLACK) canvas.drawPixel(x, y, color);
        }
    }
}
} // namespace boundary

#endif // P10_BOUNDARY_PATTERNS_H
