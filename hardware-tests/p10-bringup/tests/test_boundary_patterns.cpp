// Host tests for the SAME pure renderer used by the boundary firmware.
// Build with any C++11+ compiler; no Arduino, PlatformIO native or Unity needed.
#include "boundary_patterns.h"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
unsigned checks = 0;
void require(bool condition, const char* reason)
{
    ++checks;
    if (!condition) throw std::runtime_error(reason);
}

struct PixelSink {
    std::array<uint16_t, 1024> raw;
    std::array<unsigned, 1024> visits;
    unsigned clearCount = 0;
    bool injectShift;

    explicit PixelSink(bool shift = false) : injectShift(shift)
    {
        raw.fill(0xDEAD); // stale frame must not survive render()
        visits.fill(0);
    }
    void clearScreen()
    {
        ++clearCount;
        raw.fill(0);
        visits.fill(0);
    }
    void drawPixel(int16_t x, int16_t y, uint16_t color)
    {
        require(x >= 0 && x < 64 && y >= 0 && y < 16, "Out-of-bounds pixel emitted");
        const unsigned index = static_cast<unsigned>(y * 64 + x);
        require(++visits[index] == 1, "Duplicate logical pixel emitted");
        int16_t mapped = p10RawX(x);
        if (injectShift) mapped = static_cast<int16_t>((mapped + 1) % 64);
        raw[static_cast<unsigned>(y * 64 + mapped)] = color;
    }
    uint16_t at(int16_t x, int16_t y) const
    {
        // Independent chain-slot oracle, not a call to the mapper under test.
        return raw[static_cast<unsigned>(y * 64 + (x + 32) % 64)];
    }
    unsigned population(uint16_t color) const
    {
        unsigned total = 0;
        for (uint16_t pixel : raw) if (pixel == color) ++total;
        return total;
    }
};

void verifyOriginalCase(const PixelSink& sink)
{
    // Check far panel first so the shift control reproduces the reported symptom.
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 32; x < 64; ++x)
            require(sink.at(x, y) == boundary::WHITE,
                    "Panel 2 is not entirely white: neighboring-color strip detected");
    for (int16_t y = 0; y < 16; ++y)
        for (int16_t x = 0; x < 32; ++x)
            require(sink.at(x, y) == boundary::GREEN, "Panel 1 is not entirely green");
}

void testFrames()
{
    using namespace boundary;
    uint32_t duration = 0;
    for (uint16_t index = 0; index < kFrameCount; ++index) {
        const Frame frame = frameAt(index);
        duration += frame.holdMs;
        require(std::strcmp(name(frame.pattern), "INVALID") != 0, "Unnamed phase");
        if (index < kStaticFrames) {
            require(static_cast<uint16_t>(frame.pattern) == index, "Static phase order mismatch");
            require(frame.holdMs == 5000 && frame.coordinate == -1, "Static hold mismatch");
        } else if (index < kStaticFrames + 64) {
            require(frame.pattern == Pattern::COLUMN && frame.coordinate == index - kStaticFrames,
                    "Column order/range mismatch");
            require(frame.holdMs == 100, "Column hold mismatch");
        } else {
            require(frame.pattern == Pattern::ROW && frame.coordinate == index - kStaticFrames - 64,
                    "Row order/range mismatch");
            require(frame.holdMs == 250, "Row hold mismatch");
        }
        PixelSink sink;
        render(sink, frame);
        require(sink.clearCount == 1, "Every frame must clear the full display exactly once");
        for (int16_t y = 0; y < 16; ++y) {
            for (int16_t x = 0; x < 64; ++x) {
                const uint16_t actual = sink.at(x, y);
                // Check frame emission against the sampler, including black background.
                require(actual == colorAt(frame, x, y), "Rendered frame differs from sampler");
                if (frame.pattern == Pattern::A || frame.pattern == Pattern::B) {
                    const bool border = (x % 32 == 0) || (x % 32 == 31) || y == 0 || y == 15;
                    const bool first = x < 32;
                    uint16_t expected;
                    if (frame.pattern == Pattern::A)
                        expected = first ? (border ? RED : BLUE) : (border ? YELLOW : CYAN);
                    else
                        expected = first ? (border ? GREEN : PURPLE) : (border ? RED : YELLOW);
                    require(actual == expected, "A/B border/interior violates panel bounds");
                }
                if (frame.pattern == Pattern::X31)
                    require(actual == (x == 31 ? WHITE : BLACK), "x31 leaks into neighboring panel");
                if (frame.pattern == Pattern::X32)
                    require(actual == (x == 32 ? WHITE : BLACK), "x32 leaks into neighboring panel");
                if (frame.pattern == Pattern::SEAM)
                    require(actual == (x == 31 ? RED : (x == 32 ? GREEN : BLACK)), "Seam overlap/gap");
                if (frame.pattern == Pattern::PANEL1_ONLY)
                    require(actual == (x < 32 ? GREEN : BLACK), "Panel 1 isolation failed");
                if (frame.pattern == Pattern::PANEL2_ONLY)
                    require(actual == (x >= 32 ? WHITE : BLACK), "Panel 2 isolation failed");
                if (frame.pattern == Pattern::CHECKER || frame.pattern == Pattern::CHECKER_INVERSE) {
                    const bool lit = ((x + y) % 2 == 0) != (frame.pattern == Pattern::CHECKER_INVERSE);
                    require(actual == (lit ? WHITE : BLACK), "Checkerboard parity mismatch");
                }
                if (frame.pattern == Pattern::COLUMN)
                    require(actual == (x == frame.coordinate ? WHITE : BLACK), "Column moved/duplicated");
                if (frame.pattern == Pattern::ROW)
                    require(actual == (y == frame.coordinate ? WHITE : BLACK), "Row moved/duplicated");
            }
        }
        if (frame.pattern == Pattern::A) {
            require(sink.population(RED) == 92 && sink.population(YELLOW) == 92, "Border is not 1 pixel");
            require(sink.population(BLUE) == 420 && sink.population(CYAN) == 420, "Interior not 30x14");
        }
        if (frame.pattern == Pattern::B)
            require(sink.population(GREEN) == 92 && sink.population(PURPLE) == 420 &&
                    sink.population(RED) == 92 && sink.population(YELLOW) == 420, "B counts mismatch");
        if (frame.pattern == Pattern::CORNERS) {
            const uint16_t corners[] = {RED, GREEN, BLUE, WHITE, CYAN, PURPLE, YELLOW, ORANGE};
            const int16_t xs[] = {0, 31, 0, 31, 32, 63, 32, 63};
            const int16_t ys[] = {0, 0, 15, 15, 0, 0, 15, 15};
            for (unsigned i = 0; i < 8; ++i) {
                require(sink.at(xs[i], ys[i]) == corners[i], "Corner coordinate/color mismatch");
                for (unsigned j = i + 1; j < 8; ++j) require(corners[i] != corners[j], "Corners not distinct");
            }
            require(sink.at(0, 7) == RED && sink.at(31, 7) == GREEN &&
                    sink.at(32, 7) == YELLOW && sink.at(63, 7) == BLUE, "Edge lines mismatch");
        }
        if (frame.pattern == Pattern::GREEN_WHITE) verifyOriginalCase(sink);
        if (frame.pattern == Pattern::COLUMN) require(sink.population(WHITE) == 16, "Column count !=16");
        if (frame.pattern == Pattern::ROW) require(sink.population(WHITE) == 64, "Row count !=64");
        require(colorAt(frame, -1, 0) == BLACK && colorAt(frame, 64, 0) == BLACK &&
                colorAt(frame, 0, -1) == BLACK && colorAt(frame, 0, 16) == BLACK, "Sampler bounds failed");
    }
    require(duration == 65400, "Sequence duration is not 65.4 seconds");
}
} // namespace

int main(int argc, char** argv)
{
    try {
        if (argc == 2 && std::strcmp(argv[1], "--inject-shift") == 0) {
            PixelSink sink(true);
            boundary::render(sink, {boundary::Pattern::GREEN_WHITE, -1, 5000});
            verifyOriginalCase(sink); // MUST fail; simulated one-column transport shift
            std::fprintf(stderr, "FAIL: detector unexpectedly accepted the shifted frame\n");
            return 2;
        }
        testFrames();
        std::printf("PASS: %u checks across all 91 frames; exact borders, seam, isolation, checkerboards and sweeps\n", checks);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
