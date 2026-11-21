// Spec 03 Sections 2/4: landscape layout, mount transforms and driver writes.
#include <unity.h>
#include <array>
#include <bitset>
#include "display_color.h"
#include "display_render.h"
#include "p10_hardware.h"

void setUp() {}
void tearDown() {}

using Frame = std::bitset<64 * 16>;
static bool inRegion(int x) { return (x >= 1 && x <= 22) || (x >= 25 && x <= 38) || (x >= 41 && x <= 62); }

static Frame frame(const GamePosition& state)
{
    Frame pixels;
    renderLandscapePixels(state, [](void* context, int x, int y) {
        TEST_ASSERT_TRUE(inRegion(x) && y >= 1 && y <= 14);
        auto& pixels = *static_cast<Frame*>(context);
        TEST_ASSERT_FALSE(pixels.test(y * 64 + x)); // each lit pixel is emitted once
        pixels.set(y * 64 + x);
    }, &pixels);
    return pixels;
}

static bool has(const Frame& pixels, int x, int y) { return pixels.test(y * 64 + x); }

static int columnHeight(const Frame& pixels, int x)
{
    int lit = 0;
    for (int y = 0; y < 16; ++y) lit += has(pixels, x, y);
    return lit;
}

static Frame columns(const Frame& pixels, int first, int last)
{
    Frame part;
    for (int y = 0; y < 16; ++y) for (int x = first; x <= last; ++x) part.set(y * 64 + x, has(pixels, x, y));
    return part;
}

static GamePosition position(int left, int right, Side side, int server, GameMode mode = GameMode::DOUBLES)
{
    GamePosition state;
    state.leftScore = left;
    state.rightScore = right;
    state.servingSide = side;
    state.serverNumber = server;
    state.gameMode = mode;
    return state;
}

// Spec 03 §4b: a 6-column solid triangle, 2 rows at the tip to 12 rows at the base.
static void assertArrow(const Frame& pixels, int x, bool pointLeft)
{
    for (int col = 0; col < 6; ++col) {
        const int fromTip = pointLeft ? col : 5 - col;
        const int height = 2 * (fromTip + 1);
        TEST_ASSERT_EQUAL_INT(height, columnHeight(pixels, x + col));
        for (int y = 0; y < 16; ++y)
            TEST_ASSERT_EQUAL(y >= 7 - fromTip && y <= 8 + fromTip, has(pixels, x + col, y));
    }
}

void test_boot_witness_is_zero_left_arrow_zero()
{
    GameState state;
    const Frame pixels = frame(state);
    assertArrow(pixels, 29, true);
    for (int x = 25; x <= 38; ++x)
        if (x < 29 || x > 34) TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, x));
    // Both zeros are single 10x14 glyphs centered in their 22-column regions.
    for (int x = 1; x <= 62; ++x) {
        const bool zero = (x >= 7 && x <= 16) || (x >= 47 && x <= 56);
        if (!zero && (x <= 22 || x >= 41)) TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, x));
    }
    TEST_ASSERT_TRUE(has(pixels, 9, 1) && has(pixels, 7, 3) && has(pixels, 16, 12) && !has(pixels, 7, 1));
    TEST_ASSERT_TRUE(columns(pixels, 7, 16) << 40 == columns(pixels, 47, 56));
}

void test_four_serving_states_draw_direction_and_count()
{
    for (const Side side : {Side::LEFT, Side::RIGHT}) {
        const bool left = side == Side::LEFT;
        const Frame one = frame(position(5, 2, side, 1));
        assertArrow(one, 29, left);
        const Frame two = frame(position(5, 2, side, 2));
        assertArrow(two, 25, left);
        assertArrow(two, 33, left);
        TEST_ASSERT_EQUAL_INT(0, columnHeight(two, 31)); // the panel seam stays dark
        TEST_ASSERT_EQUAL_INT(0, columnHeight(two, 32));
        // Serving never moves the scores.
        TEST_ASSERT_TRUE(columns(one, 1, 22) == columns(two, 1, 22));
        TEST_ASSERT_TRUE(columns(one, 41, 62) == columns(two, 41, 62));
        TEST_ASSERT_TRUE(columns(one, 1, 22) == columns(frame(position(5, 2, left ? Side::RIGHT : Side::LEFT, 1)), 1, 22));
    }
}

void test_singles_always_draws_one_arrow()
{
    for (const int server : {1, 2}) {
        const Frame pixels = frame(position(3, 3, Side::RIGHT, server, GameMode::SINGLES));
        assertArrow(pixels, 29, false);
        TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, 25));
        TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, 38));
    }
}

void test_scores_stay_on_their_physical_end()
{
    const Frame a = frame(position(7, 3, Side::LEFT, 1));
    const Frame b = frame(position(7, 9, Side::RIGHT, 2));
    TEST_ASSERT_TRUE(columns(a, 1, 22) == columns(b, 1, 22));     // left 7 unchanged
    TEST_ASSERT_FALSE(columns(a, 41, 62) == columns(b, 41, 62));  // right changed
    const Frame same = frame(position(3, 7, Side::LEFT, 1));
    TEST_ASSERT_TRUE(columns(a, 1, 22) << 40 == columns(same, 41, 62)); // identical glyphs per side
}

void test_two_digits_fill_region_with_two_dark_columns()
{
    const Frame pixels = frame(position(10, 88, Side::LEFT, 1));
    // "1" at 2x: font row 0 is 00100, so left x5-6 are lit on rows 1-2.
    TEST_ASSERT_TRUE(has(pixels, 5, 1) && has(pixels, 6, 2) && !has(pixels, 4, 1));
    TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, 11));
    TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, 12));
    TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, 51));
    TEST_ASSERT_EQUAL_INT(0, columnHeight(pixels, 52));
    TEST_ASSERT_TRUE((columns(pixels, 41, 50) << 12) == columns(pixels, 53, 62)); // "8" twice
    TEST_ASSERT_TRUE(columnHeight(pixels, 41) > 0 && columnHeight(pixels, 62) > 0);
}

void test_every_score_renders_in_bounds_with_correct_digit_count()
{
    for (int score = 0; score <= 99; ++score) {
        const Frame pixels = frame(position(score, score, Side::LEFT, 2));
        int litColumns = 0;
        for (int x = 1; x <= 22; ++x) litColumns += columnHeight(pixels, x) > 0;
        TEST_ASSERT_TRUE(score < 10 ? litColumns <= 10 : litColumns > 10);
        TEST_ASSERT_TRUE(columns(pixels, 1, 22) << 40 == columns(pixels, 41, 62));
    }
}

void test_defensive_overflow_draws_two_dashes()
{
    const Frame pixels = frame(position(100, -1, Side::RIGHT, 1));
    for (const int region : {1, 41})
        for (int x = region; x < region + 22; ++x) {
            const bool dash = x - region < 10 || x - region >= 12;
            TEST_ASSERT_EQUAL_INT(dash ? 2 : 0, columnHeight(pixels, x));
            if (dash) TEST_ASSERT_TRUE(has(pixels, x, 7) && has(pixels, x, 8));
        }
}

// Independent physical models. The verified portrait witness (Spec 03 §2) saw
// raw x32..63 on top, raw x decreasing down each panel, raw y = viewer column.
struct Raw { int x; int y; };
static Raw portraitWitness(int px, int py) { return {63 - py, px}; }
static Raw clockwiseModel(int x, int y)
{
    // A clockwise quarter turn moves portrait (px, py) to landscape (63 - py, px).
    return portraitWitness(y, 63 - x);
}
static Raw counterClockwiseModel(int x, int y) { return portraitWitness(15 - y, x); }
static Raw bringUpModel(int x, int y) { return {x < 32 ? x + 32 : x - 32, y}; } // p10_geometry.h

void test_all_1024_coordinates_for_every_supported_mount()
{
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 64; ++x) {
            const Raw cw = clockwiseModel(x, y), ccw = counterClockwiseModel(x, y), bring = bringUpModel(x, y);
            TEST_ASSERT_EQUAL_INT(cw.x, landscapeDmaX(x, LandscapeMount::PortraitTurnedClockwise));
            TEST_ASSERT_EQUAL_INT(cw.y, landscapeDmaY(y, LandscapeMount::PortraitTurnedClockwise));
            TEST_ASSERT_EQUAL_INT(ccw.x, landscapeDmaX(x, LandscapeMount::PortraitTurnedCounterClockwise));
            TEST_ASSERT_EQUAL_INT(ccw.y, landscapeDmaY(y, LandscapeMount::PortraitTurnedCounterClockwise));
            TEST_ASSERT_EQUAL_INT(bring.x, landscapeDmaX(x, LandscapeMount::BringUpOrder));
            TEST_ASSERT_EQUAL_INT(bring.y, landscapeDmaY(y, LandscapeMount::BringUpOrder));
        }
    TEST_ASSERT_TRUE(INSTALLED_MOUNT == LandscapeMount::PortraitTurnedClockwise);
}

void test_wrong_mount_is_visible_in_the_boot_witness()
{
    GameState state;
    const Frame logical = frame(state);
    // What a viewer would see if the default image ran on another mount.
    for (const auto viewed : {counterClockwiseModel, bringUpModel}) {
        Frame visible;
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 64; ++x) {
            if (!has(logical, x, y)) continue;
            // Invert the other mount: find the viewer pixel that owns this raw coordinate.
            for (int vy = 0; vy < 16; ++vy) for (int vx = 0; vx < 64; ++vx) {
                const Raw raw = viewed(vx, vy);
                if (raw.x == x && raw.y == y) visible.set(vy * 64 + vx);
            }
        }
        TEST_ASSERT_FALSE(visible == logical);
        TEST_ASSERT_TRUE(columnHeight(visible, 29) != 2 || columnHeight(visible, 34) != 12);
    }
}

void test_end_game_holds_pixels_and_undo_reset_restore_them()
{
    GameState state;
    for (int i = 0; i < 11; ++i) handleScoreUp(state, Side::LEFT);
    handleNextServe(state);
    const Frame live = frame(state);
    handleEndGame(state);
    TEST_ASSERT_TRUE(live == frame(state));
    handleReset(state, GameMode::SINGLES);
    TEST_ASSERT_TRUE(frame(GameState{}) == frame(state));
    handleUndo(state);
    handleUndo(state);
    TEST_ASSERT_TRUE(live == frame(state));
}

// Spec 03 §3: every lit pixel (digits and arrows) gets the one palette color, and
// changing color changes no coordinate.
void test_driver_requests_only_one_palette_color_and_black_across_repaints()
{
    struct Display {
        std::array<uint16_t, 1024> raw;
        unsigned clears = 0;
        uint16_t expectedColor = 0;
        void clearScreen() { raw.fill(0); ++clears; }
        void drawPixel(int x, int y, uint16_t color) {
            TEST_ASSERT_EQUAL_HEX16(expectedColor, color);
            TEST_ASSERT_TRUE(x >= 0 && x < 64 && y >= 0 && y < 16);
            raw[y * 64 + x] = color;
        }
    } display;
    for (const LedColor led : LED_COLORS) {
        const uint16_t lit = ledColor565(led);
        display.expectedColor = lit;
        // Start from the other palette channels to ensure the clear leaves none behind.
        display.raw.fill(uint16_t(~lit));
        GameState state;
        for (int paint = 0; paint < 6; ++paint) {
            const Frame expected = frame(state);
            const auto before = display.clears;
            paintScoreboardFrame(display, state, lit);
            TEST_ASSERT_EQUAL(before + 1, display.clears);
            for (int y = 0; y < 16; ++y) for (int x = 0; x < 64; ++x) {
                const Raw raw = clockwiseModel(x, y);
                TEST_ASSERT_EQUAL_HEX16(has(expected, x, y) ? lit : 0, display.raw[raw.y * 64 + raw.x]);
            }
            handleScoreUp(state, Side::RIGHT);
            handleNextServe(state);
        }
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_witness_is_zero_left_arrow_zero);
    RUN_TEST(test_four_serving_states_draw_direction_and_count);
    RUN_TEST(test_singles_always_draws_one_arrow);
    RUN_TEST(test_scores_stay_on_their_physical_end);
    RUN_TEST(test_two_digits_fill_region_with_two_dark_columns);
    RUN_TEST(test_every_score_renders_in_bounds_with_correct_digit_count);
    RUN_TEST(test_defensive_overflow_draws_two_dashes);
    RUN_TEST(test_all_1024_coordinates_for_every_supported_mount);
    RUN_TEST(test_wrong_mount_is_visible_in_the_boot_witness);
    RUN_TEST(test_end_game_holds_pixels_and_undo_reset_restore_them);
    RUN_TEST(test_driver_requests_only_one_palette_color_and_black_across_repaints);
    return UNITY_END();
}
