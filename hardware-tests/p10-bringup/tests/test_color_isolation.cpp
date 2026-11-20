#include "color_isolation.h"

#include <cassert>
#include <cstdio>

int main()
{
    colorIsolation::Frame frame;
    assert(frame.phase == 0 && frame.color == 'k');
    const char colors[] = {'k', 'w', 'r', 'g', 'b'};
    const uint16_t expected[] = {0, 0xFFFF, 0xF800, 0x07E0, 0x001F};
    int hits[16][64] = {};

    for (int phase = 0; phase < 8; ++phase) {
        assert(frame.select('0' + phase));
        for (unsigned c = 0; c < sizeof(colors); ++c) {
            assert(frame.select(colors[c]));
            assert(frame.phase == phase);
            assert(!frame.select('\r') && !frame.select('\n') && !frame.select('8') && !frame.select(-1));
            assert(frame.phase == phase && frame.color == colors[c]);
            for (int row = 0; row < 8; ++row) {
                int scanCount = 0;
                for (int y = row; y < 16; y += 8) {
                    for (int start = 0; start < 64; start += 8) {
                        int groupCount = 0;
                        for (int x = start; x < start + 8; ++x) {
                            const auto value = frame.pixel(x, y);
                            assert(value == 0 || value == expected[c]);
                            assert(value == frame.pixel(x ^ 32, y));
                            if (colors[c] == 'w') {
                                assert(!value || !frame.pixel(x, y ^ 8));
                                hits[y][x] += value != 0;
                            }
                            groupCount += value != 0;
                        }
                        assert(groupCount == (colors[c] == 'k' ? 0 : 2));
                        scanCount += groupCount;
                    }
                }
                assert(scanCount == (colors[c] == 'k' ? 0 : 32));
            }
        }
    }
    for (const auto& row : hits)
        for (int count : row) assert(count == 2);

    assert(frame.select('0') && frame.select('w'));
    // Independent scope reference, first block (raw x0..31), ABC=0.
    const char bank1[] = "11000000" "00000110" "00110000" "10000001";
    const char bank2[] = "00001100" "01100000" "00000011" "00011000";
    for (int x = 0; x < 32; ++x) {
        assert((frame.pixel(x, 0) != 0) == (bank1[x] == '1'));
        assert((frame.pixel(x, 8) != 0) == (bank2[x] == '1'));
    }
    for (char block : "nf") {
        if (!block) break;
        assert(frame.select(block));
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 64; ++x) {
                const bool selected = (block == 'n') == (x >= 32);
                assert((frame.pixel(x, y) != 0) == (selected && colorIsolation::lit(x, y, 0)));
            }
        }
    }
    assert(frame.select('a') && frame.pixel(0, 0) && frame.pixel(32, 0));
    std::puts("PASS: all 1024 LEDs, 8 phases, panel/RGB isolation, scan load and scope reference");
}
