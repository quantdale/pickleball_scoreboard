// Spec 03 Sections 2/4: corrected mount, complete pixel bounds and score order.
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
        TEST_ASSERT_TRUE(y <= 14 || (y >= 17 && y <= 30) || (y >= 36 && y <= 49) || y >= 53);
        auto& pixels = *static_cast<Frame*>(context);
        TEST_ASSERT_FALSE(pixels.test(y * 16 + x));
        pixels.set(y * 16 + x);
    }, &pixels);
    return pixels;
}

static bool has(const Frame& pixels, int x, int y) { return pixels.test(y * 16 + x); }

// Independent installed-panel model, anchored to the owner-observed output
// from r180-002: its upright server/arrow in raw x32..63 is physically above
// its upright scores in raw x0..31. Do not derive this table from the new map.
constexpr int RAW_BLOCK_AT_VIEWER_PANEL[2] = {1, 0};
using DmaColumn = int16_t (*)(int16_t);

static Frame dmaFrame(const Frame& logical, DmaColumn dmaX = portraitDmaX)
{
    Frame raw;
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x)
        if (has(logical, x, y)) raw.set(portraitDmaY(x) * 64 + dmaX(y));
    return raw;
}

static Frame installedView(const Frame& raw)
{
    Frame visible;
    for (int panel = 0; panel < 2; ++panel) {
        const int rawBase = 32 * RAW_BLOCK_AT_VIEWER_PANEL[panel];
        for (int row = 0; row < 32; ++row) for (int col = 0; col < 16; ++col)
            visible.set((32 * panel + row) * 16 + col, raw.test(col * 64 + rawBase + (31 - row)));
    }
    return visible;
}

void test_previous_block_order_reproduces_owner_observed_startup()
{
    const auto logical = frame(GameState{});
    const auto before = installedView(dmaFrame(logical, [](int16_t y) -> int16_t {
        return y < 32 ? 31 - y : 95 - y; // exact r180-002 column mapping
    }));
    // Upper panel is the upright server and left arrow, not inverted zeros.
    TEST_ASSERT_TRUE(has(before, 5, 4));
    TEST_ASSERT_TRUE(has(before, 3, 17)); // server 2's solid bottom row
    TEST_ASSERT_TRUE(has(before, 5, 23)); // left arrow, below the server
    TEST_ASSERT_FALSE(has(before, 10, 23));
    TEST_ASSERT_TRUE(has(before, 5, 33)); // first zero, on the lower panel
    TEST_ASSERT_TRUE(has(before, 5, 49)); // second zero with retained layout
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x)
        TEST_ASSERT_EQUAL(has(logical, x, (y + 32) % 64), has(before, x, y));
}

void test_installed_startup_has_scores_above_server_and_arrow()
{
    const auto logical = frame(GameState{});
    const auto visible = installedView(dmaFrame(logical));
    TEST_ASSERT_TRUE_MESSAGE(has(visible, 5, 1), "First zero is missing from the physical top panel");
    TEST_ASSERT_TRUE(has(visible, 5, 17));
    TEST_ASSERT_TRUE(has(visible, 3, 49));
    TEST_ASSERT_TRUE(has(visible, 5, 55));
    TEST_ASSERT_FALSE(has(visible, 10, 55));
    TEST_ASSERT_TRUE(visible == logical);
    for (int y = 0; y < 64; ++y) {
        TEST_ASSERT_FALSE(has(visible, 0, y));
        TEST_ASSERT_FALSE(has(visible, 15, y));
    }
    for (int y : {0, 15, 16, 31, 32, 63}) for (int x = 0; x < 16; ++x)
        TEST_ASSERT_FALSE(has(visible, x, y));
}

void test_all_1024_dma_coordinates_match_installed_panel_order()
{
    Frame seen;
    for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x) {
        const int u = portraitDmaX(y), v = portraitDmaY(x);
        TEST_ASSERT_TRUE(u >= 0 && u < 64 && v >= 0 && v < 16);
        TEST_ASSERT_FALSE(seen.test(v * 64 + u));
        seen.set(v * 64 + u);
        TEST_ASSERT_EQUAL(x, v);
        TEST_ASSERT_EQUAL(RAW_BLOCK_AT_VIEWER_PANEL[y / 32], u / 32);
        TEST_ASSERT_EQUAL(31 - (y % 32), u % 32);
    }
    TEST_ASSERT_TRUE(seen.all());
    TEST_ASSERT_EQUAL(63, portraitDmaX(0));
    TEST_ASSERT_EQUAL(32, portraitDmaX(31));
    TEST_ASSERT_EQUAL(31, portraitDmaX(32));
    TEST_ASSERT_EQUAL(0, portraitDmaX(63));
    TEST_ASSERT_EQUAL(0, portraitDmaY(0));
    TEST_ASSERT_EQUAL(15, portraitDmaY(15));
}

void test_mounted_frame_preserves_asymmetric_scores_seam_and_arrows()
{
    for (bool twoDigits : {false, true}) {
        GameState state;
        state.leftScore = twoDigits ? 12 : 2;
        state.rightScore = twoDigits ? 37 : 3;
        state.serverNumber = twoDigits ? 1 : 2;
        const auto left = installedView(dmaFrame(frame(state)));
        TEST_ASSERT_TRUE(left == frame(state));
        TEST_ASSERT_TRUE(has(left, 5, 55));
        TEST_ASSERT_FALSE(has(left, 10, 55));
        handleSwitchCourts(state);
        const auto right = installedView(dmaFrame(frame(state)));
        TEST_ASSERT_TRUE(right == frame(state));
        TEST_ASSERT_FALSE(has(right, 5, 55));
        TEST_ASSERT_TRUE(has(right, 10, 55));
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x) {
            TEST_ASSERT_EQUAL(has(left, y < 53 ? x : 15 - x, y), has(right, x, y));
            if (y == 31 || y == 32) {
                TEST_ASSERT_FALSE(has(right, x, y));
            }
        }
    }
}

void test_asymmetric_frame_detects_any_panel_exchange_or_axis_reversal()
{
    GameState state;
    state.leftScore = 2;
    state.rightScore = 3;
    const auto expected = frame(state);
    const auto dma = dmaFrame(expected);

    // Independently view the two raw blocks. Five fault bits represent a block
    // exchange and independent row/column reversals on each displayed panel.
    for (int faults = 0; faults < 32; ++faults) {
        Frame visible;
        for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x) {
            const int panel = y / 32;
            const int row = y % 32;
            const int mask = faults >> (1 + 2 * panel);
            const int block = RAW_BLOCK_AT_VIEWER_PANEL[(panel + faults % 2) % 2];
            const int u = 32 * block + (mask % 2 == 0 ? 31 - row : row);
            const int v = mask % 4 < 2 ? x : 15 - x;
            visible.set(y * 16 + x, dma.test(v * 64 + u));
        }
        TEST_ASSERT_EQUAL(faults == 0, visible == expected);
        if (faults == 1) {
            // This exact fault yields upright 2 / left arrow / 2 / 3.
            for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x)
                TEST_ASSERT_EQUAL(has(expected, x, y ^ 32), has(visible, x, y));
        }
    }
}

void test_boot_glyphs_and_single_mirrored_arrow()
{
    GameState state;
    const auto left = frame(state);
    TEST_ASSERT_TRUE(has(left, 5, 1)); // upright zero: top row 01110
    TEST_ASSERT_FALSE(has(left, 3, 1));
    TEST_ASSERT_TRUE(has(left, 5, 17));
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

void test_boot_zeros_are_separate_full_height_glyphs()
{
    const auto pixels = frame(GameState{});
    auto rowLit = [&](int y) {
        for (int x = 0; x < 16; ++x) if (has(pixels, x, y)) return true;
        return false;
    };
    // Two intact 14-row digits with two completely dark rows in between.
    for (int y = 1; y <= 14; ++y) TEST_ASSERT_TRUE(rowLit(y));
    TEST_ASSERT_FALSE(rowLit(15));
    TEST_ASSERT_FALSE(rowLit(16));
    for (int y = 17; y <= 30; ++y) TEST_ASSERT_TRUE(rowLit(y));
    TEST_ASSERT_FALSE(rowLit(31));
    TEST_ASSERT_FALSE(rowLit(32));
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
        const bool dashRow = y == 7 || y == 8 || y == 23 || y == 24;
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
        TEST_ASSERT_EQUAL(mode == GameMode::DOUBLES ? 1 : 2, state.serverNumber);
        const auto sideOut = frame(state);
        if (mode == GameMode::DOUBLES) {
            handleRallyWonLeft(state); // partner takes over; only the server band may change
            const auto partner = frame(state);
            TEST_ASSERT_TRUE(partner != sideOut);
            for (int y = 0; y < 64; ++y) for (int x = 0; x < 16; ++x) {
                if (y < 36 || y >= 53) TEST_ASSERT_EQUAL(has(partner, x, y), has(sideOut, x, y));
            }
        }
        TEST_ASSERT_TRUE(has(sideOut, 10, 55));
        for (int y = 1; y <= 14; ++y) for (int x = 0; x < 16; ++x) {
            TEST_ASSERT_EQUAL(has(before, x, y + 16), has(sideOut, x, y));
            TEST_ASSERT_EQUAL(has(before, x, y), has(sideOut, x, y + 16));
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
    RUN_TEST(test_previous_block_order_reproduces_owner_observed_startup);
    RUN_TEST(test_installed_startup_has_scores_above_server_and_arrow);
    RUN_TEST(test_all_1024_dma_coordinates_match_installed_panel_order);
    RUN_TEST(test_mounted_frame_preserves_asymmetric_scores_seam_and_arrows);
    RUN_TEST(test_asymmetric_frame_detects_any_panel_exchange_or_axis_reversal);
    RUN_TEST(test_boot_glyphs_and_single_mirrored_arrow);
    RUN_TEST(test_boot_zeros_are_separate_full_height_glyphs);
    RUN_TEST(test_all_scores_in_each_band_stay_in_bounds);
    RUN_TEST(test_overflow_draws_two_dashes_in_each_score_band);
    RUN_TEST(test_switch_changes_only_arrow_but_side_out_reorders_scores);
    RUN_TEST(test_end_game_freezes_pixels_reset_and_undo_restore_them);
    return UNITY_END();
}
