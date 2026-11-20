// Spec 02 §§4–5 and Spec 03 §2a: operator brightness, independent of scoring.
#include <unity.h>
#include <string>
#include "ble_command_parser.h"
#include "state_payload.h"
#include "../test_state_asserts.h"

void setUp() {}
void tearDown() {}

static BoardCommandEffect apply(const std::string& command, GameState& state, BrightnessSetting& brightness,
                                uint32_t now = 0)
{
    return applyBoardCommand(command.data(), command.size(), state, brightness, now);
}

void test_default_is_level_one_at_twelve()
{
    BrightnessSetting brightness;
    TEST_ASSERT_EQUAL_UINT8(1, brightness.level());
    TEST_ASSERT_EQUAL_UINT8(12, brightnessValue(brightness.level()));
    TEST_ASSERT_EQUAL_UINT8(4, BRIGHTNESS_MAX_LEVEL); // conservative uncertified cap
    for (uint8_t level = 2; level <= BRIGHTNESS_LADDER_LEVELS; ++level)
        TEST_ASSERT_TRUE(brightnessValue(level) > brightnessValue(level - 1));
    TEST_ASSERT_TRUE(brightnessValue(BRIGHTNESS_LADDER_LEVELS) <= 128);
}

void test_only_capped_brightness_levels_are_commands()
{
    int brightnessCommands = 0, boardCommands = 0;
    for (int a = 0; a < 256; ++a) {
        const char one[] = {static_cast<char>(a)};
        TEST_ASSERT_EQUAL_UINT8(0, brightnessCommandLevel(one, 1));
        boardCommands += isBoardCommand(one, 1);
        for (int b = 0; b < 256; ++b) {
            const char two[] = {static_cast<char>(a), static_cast<char>(b)};
            const uint8_t level = brightnessCommandLevel(two, 2);
            if (level) {
                ++brightnessCommands;
                TEST_ASSERT_EQUAL_CHAR('B', two[0]);
                TEST_ASSERT_EQUAL_UINT8(two[1] - '0', level);
                TEST_ASSERT_FALSE(isScoringCommand(two, 2)); // never collides with scoring
            }
            boardCommands += isBoardCommand(two, 2);
        }
    }
    TEST_ASSERT_EQUAL_INT(BRIGHTNESS_MAX_LEVEL, brightnessCommands);
    TEST_ASSERT_EQUAL_INT(10 + BRIGHTNESS_MAX_LEVEL, boardCommands);
}

void test_malformed_brightness_commands_change_nothing()
{
    GameState state;
    BrightnessSetting brightness;
    apply("+L", state, brightness);
    for (const char* command : {"B", "B0", "B5", "B8", "B9", "b2", "B+", "B-", "BB", "2B", "B10", "B22", "B,2"}) {
        TEST_ASSERT_FALSE(isBoardCommand(command, std::string(command).size()));
        const auto effect = apply(command, state, brightness);
        TEST_ASSERT_FALSE(effect.brightnessChanged || effect.visibleChange || effect.undoStatusChanged);
    }
    const auto withNul = apply(std::string("B2\0", 3), state, brightness);
    TEST_ASSERT_FALSE(withNul.brightnessChanged);
    TEST_ASSERT_EQUAL_UINT8(1, brightness.level());
    assertState(state, 1, 0, Side::LEFT, 1, false);
    TEST_ASSERT_EQUAL_UINT(1, state.historyDepth());
}

void test_brightness_never_touches_score_serve_or_undo()
{
    GameState state;
    BrightnessSetting brightness;
    apply("+L", state, brightness);
    apply("+R", state, brightness);
    apply("S", state, brightness);
    const std::string before = serializeManualStatePayload(state);
    const std::size_t depth = state.historyDepth();
    for (const char* command : {"B2", "B3", "B4", "B1"}) {
        const auto effect = apply(command, state, brightness);
        TEST_ASSERT_TRUE(effect.brightnessChanged);
        TEST_ASSERT_FALSE(effect.visibleChange || effect.undoStatusChanged);
        TEST_ASSERT_EQUAL_STRING(before.c_str(), serializeManualStatePayload(state).c_str());
        TEST_ASSERT_EQUAL_UINT(depth, state.historyDepth());
    }
    apply("B3", state, brightness);
    // Undo, End Game and Reset neither restore nor reset brightness.
    TEST_ASSERT_TRUE(apply("U", state, brightness).visibleChange);
    assertState(state, 1, 1, Side::LEFT, 1, false);
    apply("E", state, brightness);
    TEST_ASSERT_TRUE(apply("B4", state, brightness).brightnessChanged); // allowed after End Game
    apply("0S", state, brightness);
    TEST_ASSERT_EQUAL_UINT8(4, brightness.level());
    while (state.canUndo()) apply("U", state, brightness);
    TEST_ASSERT_EQUAL_UINT8(4, brightness.level());
    assertState(state, 0, 0, Side::LEFT, 1, false);
}

void test_repeated_level_is_ignored_and_boundaries_hold()
{
    GameState state;
    BrightnessSetting brightness;
    TEST_ASSERT_FALSE(apply("B1", state, brightness).brightnessChanged); // already the default
    const std::string top = std::string("B") + char('0' + BRIGHTNESS_MAX_LEVEL);
    const std::string over = std::string("B") + char('0' + BRIGHTNESS_MAX_LEVEL + 1);
    TEST_ASSERT_TRUE(apply(top, state, brightness).brightnessChanged);
    TEST_ASSERT_FALSE(apply(top, state, brightness).brightnessChanged);
    TEST_ASSERT_FALSE(apply(over, state, brightness).brightnessChanged);
    TEST_ASSERT_EQUAL_UINT8(BRIGHTNESS_MAX_LEVEL, brightness.level());
    TEST_ASSERT_TRUE(apply("B1", state, brightness).brightnessChanged);
    TEST_ASSERT_FALSE(apply("B0", state, brightness).brightnessChanged);
    TEST_ASSERT_EQUAL_UINT8(1, brightness.level());
}

void test_rapid_interleaved_adjustments_match_independent_model()
{
    GameState state;
    BrightnessSetting brightness;
    int left = 0, right = 0, level = 1, history = 0;
    uint32_t rng = 0xB417u;
    for (uint32_t i = 0; i < 20000; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const unsigned pick = (rng >> 24) % 12;
        if (pick < 9) {
            const int requested = int(pick % 9) + 1; // includes levels above the cap
            const std::string command = std::string("B") + char('0' + requested);
            const auto effect = apply(command, state, brightness, i);
            const bool valid = requested <= BRIGHTNESS_MAX_LEVEL;
            TEST_ASSERT_EQUAL(valid && requested != level, effect.brightnessChanged);
            if (valid) level = requested;
        } else {
            // Spec 01 §3: a clamped input is ignored and records no history.
            const char* command = pick == 9 ? "+L" : pick == 10 ? "-R" : "+R";
            int& score = pick == 9 ? left : right;
            const int next = pick == 10 ? (score > 0 ? score - 1 : 0) : (score < 99 ? score + 1 : 99);
            history += next != score;
            score = next;
            TEST_ASSERT_FALSE(apply(command, state, brightness, i).brightnessChanged);
        }
        TEST_ASSERT_EQUAL_UINT8(level, brightness.level());
        TEST_ASSERT_TRUE(level >= 1 && level <= BRIGHTNESS_MAX_LEVEL);
        TEST_ASSERT_EQUAL(left, state.leftScore);
        TEST_ASSERT_EQUAL(right, state.rightScore);
        TEST_ASSERT_EQUAL_UINT(std::size_t(history < 2048 ? history : 2048), state.historyDepth());
    }
}

void test_save_waits_for_a_settled_level()
{
    BrightnessSetting brightness;
    const uint32_t delay = BrightnessSetting::SAVE_DELAY_MS;
    TEST_ASSERT_FALSE(brightness.saveDue(100000)); // default is never written unprompted
    brightness.apply(2, 1000);
    brightness.apply(3, 1500); // rapid taps restart the delay
    TEST_ASSERT_FALSE(brightness.saveDue(1500 + delay - 1));
    TEST_ASSERT_TRUE(brightness.saveDue(1500 + delay));
    brightness.saved(false, 1500 + delay); // failed write: retry after a full delay
    TEST_ASSERT_FALSE(brightness.saveDue(1500 + 2 * delay - 1));
    TEST_ASSERT_TRUE(brightness.saveDue(1500 + 2 * delay));
    brightness.saved(true, 1500 + 2 * delay);
    TEST_ASSERT_FALSE(brightness.saveDue(UINT32_MAX));
    brightness.apply(4, 20000);
    brightness.apply(3, 20100); // back to the stored level: nothing to write
    TEST_ASSERT_FALSE(brightness.saveDue(20100 + delay));
    brightness.apply(2, UINT32_MAX - 10); // millis() wraparound
    TEST_ASSERT_FALSE(brightness.saveDue(5));
    TEST_ASSERT_TRUE(brightness.saveDue(delay));
}

void test_restore_defaults_invalid_and_caps_records()
{
    TEST_ASSERT_EQUAL_UINT8(1, restoredBrightnessLevel(false, 3));
    TEST_ASSERT_EQUAL_UINT8(1, restoredBrightnessLevel(true, 0));
    TEST_ASSERT_EQUAL_UINT8(1, restoredBrightnessLevel(true, BRIGHTNESS_LADDER_LEVELS + 1));
    TEST_ASSERT_EQUAL_UINT8(1, restoredBrightnessLevel(true, 255));
    for (uint8_t stored = 1; stored <= BRIGHTNESS_LADDER_LEVELS; ++stored) {
        const uint8_t expected = stored < BRIGHTNESS_MAX_LEVEL ? stored : BRIGHTNESS_MAX_LEVEL;
        TEST_ASSERT_EQUAL_UINT8(expected, restoredBrightnessLevel(true, stored));
    }
    BrightnessSetting restored(restoredBrightnessLevel(true, 3));
    TEST_ASSERT_EQUAL_UINT8(3, restored.level());
    TEST_ASSERT_FALSE(restored.saveDue(UINT32_MAX)); // restoring is not a change
}

void test_brightness_report_is_distinct_from_state()
{
    TEST_ASSERT_EQUAL_STRING("B,1,4", serializeBrightnessPayload(1).c_str());
    for (uint8_t level = 1; level <= BRIGHTNESS_MAX_LEVEL; ++level) {
        const std::string payload = serializeBrightnessPayload(level);
        TEST_ASSERT_TRUE(payload.size() <= 20); // Spec 02 §5: default ATT MTU
        TEST_ASSERT_EQUAL_CHAR('B', payload[0]); // a state payload always starts with a digit
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_default_is_level_one_at_twelve);
    RUN_TEST(test_only_capped_brightness_levels_are_commands);
    RUN_TEST(test_malformed_brightness_commands_change_nothing);
    RUN_TEST(test_brightness_never_touches_score_serve_or_undo);
    RUN_TEST(test_repeated_level_is_ignored_and_boundaries_hold);
    RUN_TEST(test_rapid_interleaved_adjustments_match_independent_model);
    RUN_TEST(test_save_waits_for_a_settled_level);
    RUN_TEST(test_restore_defaults_invalid_and_caps_records);
    RUN_TEST(test_brightness_report_is_distinct_from_state);
    return UNITY_END();
}
