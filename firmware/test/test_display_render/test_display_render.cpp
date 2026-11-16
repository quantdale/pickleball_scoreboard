// Spec 03 Sections 2/4: bounds/gutters, scaled glyphs and exact panel mapping.
#include <unity.h>
#include <set>
#include <utility>
#include "display_render_logic.h"
#include "p10_hardware.h"

void setUp() {}
void tearDown() {}

static bool has(const std::vector<RenderPixel>& pixels, int x, int y)
{
    for (const auto& p : pixels) if (p.x == x && p.y == y) return true;
    return false;
}

void test_initial_scaled_scores_and_two_left_arrows()
{
    GameState state;
    const auto pixels = computeRenderedPixels(state);
    TEST_ASSERT_TRUE(has(pixels, 16, 1)); // zero row 01110 scaled at x14
    TEST_ASSERT_TRUE(has(pixels, 17, 2));
    TEST_ASSERT_TRUE(has(pixels, 42, 1));
    TEST_ASSERT_TRUE(has(pixels, 1, 4)); // left arrow shaft, top2+row2
    TEST_ASSERT_TRUE(has(pixels, 1, 11)); // second arrow
    TEST_ASSERT_FALSE(has(pixels, 58, 4));
}

void test_one_right_arrow_and_end_freeze()
{
    GameState state;
    state.servingSide = Side::RIGHT;
    state.serverNumber = 1;
    auto pixels = computeRenderedPixels(state);
    TEST_ASSERT_TRUE(has(pixels, 62, 7));
    TEST_ASSERT_FALSE(has(pixels, 1, 4));
    state.gameEnded = true;
    const auto ended = computeRenderedPixels(state);
    TEST_ASSERT_EQUAL(pixels.size(), ended.size());
    for (const auto& pixel : pixels) TEST_ASSERT_TRUE(has(ended, pixel.x, pixel.y));
}

void test_all_scores_sides_servers_bounds_unique_gutters()
{
    for (int score = 0; score <= 99; ++score) {
        for (Side side : {Side::LEFT, Side::RIGHT}) {
            for (int server : {1, 2}) {
                GameState state;
                state.leftScore = score;
                state.rightScore = 99 - score;
                state.servingSide = side;
                state.serverNumber = server;
                const auto pixels = computeRenderedPixels(state);
                std::set<std::pair<int, int>> seen;
                for (const auto& p : pixels) {
                    TEST_ASSERT_TRUE(p.x > 0 && p.x < 63 && p.y > 0 && p.y < 15);
                    TEST_ASSERT_FALSE(p.x >= 30 && p.x <= 33);
                    TEST_ASSERT_EQUAL_HEX16(0xFFFF, p.color);
                    TEST_ASSERT_TRUE(seen.insert({p.x, p.y}).second);
                    TEST_ASSERT_TRUE(physicalDisplayX(static_cast<int16_t>(p.x)) >= 0);
                    TEST_ASSERT_TRUE(physicalDisplayX(static_cast<int16_t>(p.x)) < 64);
                }
            }
        }
    }
}

void test_double_digits_10_25_99()
{
    for (int score : {10, 25, 99}) {
        GameState state;
        state.leftScore = score;
        state.rightScore = score;
        const auto pixels = computeRenderedPixels(state);
        // Both score blocks contain pixels at their final digit's bottom row.
        bool left = false, right = false;
        for (const auto& p : pixels) {
            left |= p.x >= 20 && p.x <= 29 && p.y == 14;
            right |= p.x >= 46 && p.x <= 55 && p.y == 14;
        }
        TEST_ASSERT_TRUE(left);
        TEST_ASSERT_TRUE(right);
    }
}

void test_overflow_is_dashes_not_wrap()
{
    GameState state;
    state.leftScore = 100;
    state.rightScore = -1;
    const auto pixels = computeRenderedPixels(state);
    TEST_ASSERT_TRUE(has(pixels, 8, 7));
    TEST_ASSERT_TRUE(has(pixels, 29, 8));
    TEST_ASSERT_TRUE(has(pixels, 34, 7));
    TEST_ASSERT_FALSE(has(pixels, 8, 1));
}

void test_panel_boundary_permutation()
{
    std::set<int> raw;
    for (int16_t x = 0; x < 64; ++x) {
        raw.insert(physicalDisplayX(x));
        TEST_ASSERT_EQUAL(x % 32, physicalDisplayX(x) % 32);
    }
    TEST_ASSERT_EQUAL(64, raw.size());
    TEST_ASSERT_EQUAL(63, physicalDisplayX(31));
    TEST_ASSERT_EQUAL(0, physicalDisplayX(32));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_initial_scaled_scores_and_two_left_arrows);
    RUN_TEST(test_one_right_arrow_and_end_freeze);
    RUN_TEST(test_all_scores_sides_servers_bounds_unique_gutters);
    RUN_TEST(test_double_digits_10_25_99);
    RUN_TEST(test_overflow_is_dashes_not_wrap);
    RUN_TEST(test_panel_boundary_permutation);
    return UNITY_END();
}
