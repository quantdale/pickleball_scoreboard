// Spec 03 Sections 2/4: clockwise mount, complete pixel bounds and score order.
#include <unity.h>
#include <bitset>
#include "display_render_logic.h"
#include "p10_hardware.h"

void setUp() {}
void tearDown() {}

using Frame = std::bitset<16 * 64>;
static Frame frame(const GameState& state)
{
    Frame pixels;
    renderPortraitPixels(state, [](void* context, int x, int y) {
        TEST_ASSERT_TRUE(x >= 2 && x <= 13 && y >= 1 && y <= 62);
        TEST_ASSERT_TRUE(y < 30 || y >= 36);
        auto& pixels = *static_cast<Frame*>(context);
        TEST_ASSERT_FALSE(pixels.test(y * 16 + x));
        pixels.set(y * 16 + x);
    }, &pixels);
    return pixels;
}

static bool has(const Frame& pixels, int x, int y) { return pixels.test(y * 16 + x); }

void test_clockwise_map_is_bijective_and_keeps_original_left_on_top()
{
    Frame seen;
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x) {
        const int u = portraitDmaX(y), v = portraitDmaY(x);
        TEST_ASSERT_TRUE(u >= 0 && u < 64 && v >= 0 && v < 16);
        TEST_ASSERT_FALSE(seen.test(v * 64 + u));
        seen.set(v * 64 + u);
        // Independently rotate raw coordinates back into the viewer's frame.
        TEST_ASSERT_EQUAL(x, 15 - v);
        TEST_ASSERT_EQUAL(y, u < 32 ? u + 32 : u - 32);
        TEST_ASSERT_EQUAL(y < 32, u >= 32);
    }
    TEST_ASSERT_TRUE(seen.all());
    TEST_ASSERT_EQUAL(32, portraitDmaX(0));
    TEST_ASSERT_EQUAL(63, portraitDmaX(31));
    TEST_ASSERT_EQUAL(0, portraitDmaX(32));
    TEST_ASSERT_EQUAL(31, portraitDmaX(63));
    TEST_ASSERT_EQUAL(15, portraitDmaY(0));
    TEST_ASSERT_EQUAL(0, portraitDmaY(15));
}

void test_boot_glyphs_and_single_mirrored_arrow()
{
    GameState state;
    const auto left = frame(state);
    TEST_ASSERT_TRUE(has(left, 5, 1)); // upright zero: top row 01110
    TEST_ASSERT_FALSE(has(left, 3, 1));
    TEST_ASSERT_TRUE(has(left, 5, 16));
    TEST_ASSERT_TRUE(has(left, 5, 36)); // server 2: top row 01110
    TEST_ASSERT_TRUE(has(left, 3, 49)); // server 2: bottom row 11111
    state.serverNumber = 1;
    state.servingSide = Side::RIGHT;
    const auto right = frame(state);
    TEST_ASSERT_FALSE(has(right, 5, 36)); // server 1: top row 00100
    TEST_ASSERT_TRUE(has(right, 7, 36));
    int arrowPixels = 0;
    for (int y = 50; y < 64; ++y) for (int x = 0; x < 16; ++x) {
        TEST_ASSERT_EQUAL(has(left, x, y), has(right, 15 - x, y));
        arrowPixels += has(left, x, y);
    }
    TEST_ASSERT_EQUAL(36, arrowPixels);
    TEST_ASSERT_TRUE(has(left, 5, 55));
    TEST_ASSERT_TRUE(has(right, 10, 55));
}

void test_all_scores_in_each_band_stay_in_bounds()
{
    for (int score = 0; score <= 99; ++score) for (int server = 1; server <= 2; ++server) {
        GameState state;
        state.leftScore = score;
        state.rightScore = 99 - score;
        state.serverNumber = server;
        TEST_ASSERT_TRUE(frame(state).any());
        state.servingSide = Side::RIGHT;
        TEST_ASSERT_TRUE(frame(state).any());
    }
}

void test_overflow_draws_two_dashes_in_each_score_band()
{
    GameState state;
    state.leftScore = 100;
    state.rightScore = -1;
    const auto pixels = frame(state);
    for (int y = 0; y < 32; ++y) for (int x = 0; x < 16; ++x) {
        const bool dashRow = y == 7 || y == 8 || y == 22 || y == 23;
        TEST_ASSERT_EQUAL(dashRow && ((x >= 2 && x <= 6) || (x >= 9 && x <= 13)), has(pixels, x, y));
    }
}

void test_switch_changes_only_arrow_but_side_out_reorders_scores()
{
    for (const GameMode mode : {GameMode::DOUBLES, GameMode::SINGLES}) {
        GameState state;
        initGameState(state, Side::LEFT, mode);
        state.leftScore = 2;
        state.rightScore = 3;
        const auto before = frame(state);
        handleSwitchCourts(state);
        const auto switched = frame(state);
        for (int y = 0; y < 53; ++y) for (int x = 0; x < 16; ++x)
            TEST_ASSERT_EQUAL(has(before, x, y), has(switched, x, y));
        TEST_ASSERT_TRUE(has(switched, 10, 55));
        handleUndo(state);
        TEST_ASSERT_TRUE(before == frame(state));
        handleRallyWonRight(state);
        TEST_ASSERT_EQUAL(2, state.serverNumber);
        const auto sideOut = frame(state);
        TEST_ASSERT_TRUE(has(sideOut, 10, 55));
        for (int y = 1; y <= 14; ++y) for (int x = 0; x < 16; ++x) {
            TEST_ASSERT_EQUAL(has(before, x, y + 15), has(sideOut, x, y));
            TEST_ASSERT_EQUAL(has(before, x, y), has(sideOut, x, y + 15));
        }
    }
}

void test_end_game_freezes_pixels_reset_and_undo_restore_them()
{
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    const auto before = frame(state);
    handleEndGame(state);
    handleSwitchCourts(state);
    handleRallyWonRight(state);
    TEST_ASSERT_TRUE(before == frame(state));
    handleReset(state, Side::RIGHT, GameMode::SINGLES);
    TEST_ASSERT_TRUE(before != frame(state));
    handleUndo(state);
    TEST_ASSERT_TRUE(state.gameEnded);
    TEST_ASSERT_TRUE(before == frame(state));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_clockwise_map_is_bijective_and_keeps_original_left_on_top);
    RUN_TEST(test_boot_glyphs_and_single_mirrored_arrow);
    RUN_TEST(test_all_scores_in_each_band_stay_in_bounds);
    RUN_TEST(test_overflow_draws_two_dashes_in_each_score_band);
    RUN_TEST(test_switch_changes_only_arrow_but_side_out_reorders_scores);
    RUN_TEST(test_end_game_freezes_pixels_reset_and_undo_restore_them);
    return UNITY_END();
}
