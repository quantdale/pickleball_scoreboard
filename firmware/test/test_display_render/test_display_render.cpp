// Unit tests for the pure-logic display renderer.
// Verifies that computeRenderedPixels() produces the expected lit pixels for
// representative game states, independent of actual HUB75 hardware.
// See docs/specs/03-display-rendering.md.

#include <unity.h>
#include <algorithm>
#include "display_render_logic.h"
#include "game_state.h"

static constexpr uint16_t WHITE_RGB565 = 0xFFFF;

void setUp(void) {}
void tearDown(void) {}

static bool hasPixel(const std::vector<RenderPixel>& pixels, int x, int y, uint16_t color) {
    return std::any_of(pixels.begin(), pixels.end(), [&](const RenderPixel& p) {
        return p.x == x && p.y == y && p.color == color;
    });
}

static bool hasWhitePixel(const std::vector<RenderPixel>& pixels, int x, int y) {
    return hasPixel(pixels, x, y, WHITE_RGB565);
}

static int countPixels(const std::vector<RenderPixel>& pixels) {
    return static_cast<int>(pixels.size());
}

// '0' glyph left edge at x=13, top at y=15.
static void assertZeroGlyphAt(const std::vector<RenderPixel>& pixels, int leftX, int topY) {
    // Top row: 01110 -> columns 1,2,3 lit
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 1, topY));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 2, topY));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 3, topY));
    // Bottom row: 01110
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 1, topY + 6));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 2, topY + 6));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 3, topY + 6));
    // Upper-left diagonal pixel should be unlit inside the zero.
    TEST_ASSERT_FALSE(hasWhitePixel(pixels, leftX + 2, topY + 3));
}

// ARROW_RIGHT glyph left edge at x, top at y.
static void assertArrowRightGlyphAt(const std::vector<RenderPixel>& pixels, int leftX, int topY) {
    // Shaft runs down column 5 (0-indexed from left of the 9-wide bitmap).
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 5, topY + 0));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 5, topY + 6));
    // Arrow head center row (row 3) is fully lit.
    for (int col = 0; col < 9; ++col) {
        TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + col, topY + 3));
    }
}

// ARROW_LEFT glyph left edge at x, top at y.
static void assertArrowLeftGlyphAt(const std::vector<RenderPixel>& pixels, int leftX, int topY) {
    // Shaft runs down column 3.
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 3, topY + 0));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + 3, topY + 6));
    // Arrow head center row is fully lit.
    for (int col = 0; col < 9; ++col) {
        TEST_ASSERT_TRUE(hasWhitePixel(pixels, leftX + col, topY + 3));
    }
}

void test_initial_state_0_0_2_left(void) {
    GameState state;
    initGameState(state, Side::LEFT);

    const auto pixels = computeRenderedPixels(state);

    // Center divider should be present (sparse dotted line).
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 32, 4));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 32, 28));

    // Both scores are 0, centered in each half.
    assertZeroGlyphAt(pixels, 13, 15);   // left score
    assertZeroGlyphAt(pixels, 45, 15);   // right score

    // Left serving, server number 2 -> two left arrows above left score.
    assertArrowLeftGlyphAt(pixels, 6, 3);   // first arrow
    assertArrowLeftGlyphAt(pixels, 16, 3);  // second arrow, 1px gap

    // Right side should have no arrow.
    TEST_ASSERT_FALSE(hasWhitePixel(pixels, 43, 3));
}

void test_left_serving_server_one(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    state.serverNumber = 1;

    const auto pixels = computeRenderedPixels(state);

    // Single left arrow centered over left half (left edge = 11).
    assertArrowLeftGlyphAt(pixels, 11, 3);
    // No second arrow.
    TEST_ASSERT_FALSE(hasWhitePixel(pixels, 21, 3));
}

void test_right_serving_server_one(void) {
    GameState state;
    initGameState(state, Side::RIGHT);
    state.serverNumber = 1;

    const auto pixels = computeRenderedPixels(state);

    // Both scores are 0, centered in each half.
    assertZeroGlyphAt(pixels, 45, 15);   // right score
    assertZeroGlyphAt(pixels, 13, 15);   // left score

    // Single right arrow centered over right half (left edge = 43).
    assertArrowRightGlyphAt(pixels, 43, 3);
    // No arrow on left.
    TEST_ASSERT_FALSE(hasWhitePixel(pixels, 11, 3));
}

void test_double_digit_scores_and_two_right_arrows(void) {
    GameState state;
    initGameState(state, Side::RIGHT);
    state.leftScore = 10;
    state.rightScore = 25;
    state.serverNumber = 2;

    const auto pixels = computeRenderedPixels(state);

    // Left score "10" spans x=10..20, top y=15.
    // '1' at x=10, '0' at x=16.
    // Top of '1' row: 00100 -> column 2 lit.
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 12, 15));
    // Confirm '0' is present next to it.
    assertZeroGlyphAt(pixels, 16, 15);

    // Right score "25" spans x=42..52, top y=15.
    // '2' at x=42, '5' at x=48.
    // Top of '2' row: 01110 -> columns 1,2,3 lit.
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 43, 15));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 44, 15));
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 45, 15));

    // Two right arrows above right half: left edges 38 and 48.
    assertArrowRightGlyphAt(pixels, 38, 3);
    assertArrowRightGlyphAt(pixels, 48, 3);
}

void test_end_game_renders_final_state(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    state.leftScore = 4;
    state.rightScore = 2;
    state.gameEnded = true;

    const auto pixels = computeRenderedPixels(state);

    // Scores still rendered.
    assertZeroGlyphAt(pixels, 45, 15); // right score '2'
    // Divider still rendered.
    TEST_ASSERT_TRUE(hasWhitePixel(pixels, 32, 16));
    // Serving arrow still rendered.
    assertArrowLeftGlyphAt(pixels, 6, 3);
}

void test_pixels_are_within_canvas_bounds(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    state.leftScore = 99;
    state.rightScore = 99;
    state.serverNumber = 2;

    const auto pixels = computeRenderedPixels(state);

    for (const auto& p : pixels) {
        TEST_ASSERT_GREATER_OR_EQUAL(0, p.x);
        TEST_ASSERT_LESS_THAN(64, p.x);
        TEST_ASSERT_GREATER_OR_EQUAL(0, p.y);
        TEST_ASSERT_LESS_THAN(32, p.y);
    }
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_initial_state_0_0_2_left);
    RUN_TEST(test_left_serving_server_one);
    RUN_TEST(test_right_serving_server_one);
    RUN_TEST(test_double_digit_scores_and_two_right_arrows);
    RUN_TEST(test_end_game_renders_final_state);
    RUN_TEST(test_pixels_are_within_canvas_bounds);

    UNITY_END();
}

void loop() {
    // Empty; tests run once in setup().
}
