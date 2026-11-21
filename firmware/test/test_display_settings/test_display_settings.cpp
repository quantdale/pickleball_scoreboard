// Spec 02 §§4–5a and Spec 03 §§2a/3: operator brightness and color, independent of scoring.
#include <unity.h>
#include <string>
#include "ble_command_parser.h"
#include "state_payload.h"
#include "../test_state_asserts.h"

void setUp() {}
void tearDown() {}

static BoardCommandEffect apply(const std::string& command, GameState& state, DisplaySettings& brightness,
                                uint32_t now = 0)
{
    return applyBoardCommand(command.data(), command.size(), state, brightness, now);
}

void test_default_is_level_one_at_twelve()
{
    DisplaySettings brightness;
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
    TEST_ASSERT_EQUAL_INT(10 + BRIGHTNESS_MAX_LEVEL + 6, boardCommands);
}

void test_malformed_brightness_commands_change_nothing()
{
    GameState state;
    DisplaySettings brightness;
    apply("+L", state, brightness);
    for (const char* command : {"B", "B0", "B5", "B8", "B9", "b2", "B+", "B-", "BB", "2B", "B10", "B22", "B,2"}) {
        TEST_ASSERT_FALSE(isBoardCommand(command, std::string(command).size()));
        const auto effect = apply(command, state, brightness);
        TEST_ASSERT_FALSE(effect.brightnessChanged || effect.colorChanged || effect.visibleChange || effect.undoStatusChanged);
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
    DisplaySettings brightness;
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
    DisplaySettings brightness;
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
    DisplaySettings brightness;
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
        TEST_ASSERT_EQUAL_UINT(std::size_t(history) < GameState::HISTORY_POSITIONS ? std::size_t(history) : GameState::HISTORY_POSITIONS,
                               state.historyDepth());
    }
}

void test_save_waits_for_a_settled_level()
{
    DisplaySettings brightness;
    const uint32_t delay = DisplaySettings::SAVE_DELAY_MS;
    TEST_ASSERT_FALSE(brightness.saveDue(100000)); // default is never written unprompted
    brightness.applyLevel(2, 1000);
    brightness.applyLevel(3, 1500); // rapid taps restart the delay
    TEST_ASSERT_FALSE(brightness.saveDue(1500 + delay - 1));
    TEST_ASSERT_TRUE(brightness.saveDue(1500 + delay));
    brightness.saved(false, 1500 + delay); // failed write: retry after a full delay
    TEST_ASSERT_FALSE(brightness.saveDue(1500 + 2 * delay - 1));
    TEST_ASSERT_TRUE(brightness.saveDue(1500 + 2 * delay));
    brightness.saved(true, 1500 + 2 * delay);
    TEST_ASSERT_FALSE(brightness.saveDue(UINT32_MAX));
    brightness.applyLevel(4, 20000);
    brightness.applyLevel(3, 20100); // back to the stored level: nothing to write
    TEST_ASSERT_FALSE(brightness.saveDue(20100 + delay));
    brightness.applyLevel(2, UINT32_MAX - 10); // millis() wraparound
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
    DisplaySettings restored(restoredBrightnessLevel(true, 3));
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

void test_palette_is_full_channel_with_white_default()
{
    TEST_ASSERT_EQUAL_UINT(6, sizeof(LED_COLORS) / sizeof(LED_COLORS[0]));
    TEST_ASSERT_TRUE(DisplaySettings().color() == LedColor::White);
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, ledColor565(LedColor::White));
    TEST_ASSERT_EQUAL_HEX16(0xF800, ledColor565(LedColor::Red));
    TEST_ASSERT_EQUAL_HEX16(0xFFE0, ledColor565(LedColor::Yellow));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, ledColor565(LedColor::Green));
    TEST_ASSERT_EQUAL_HEX16(0x07FF, ledColor565(LedColor::Cyan));
    TEST_ASSERT_EQUAL_HEX16(0x001F, ledColor565(LedColor::Blue));
    for (const LedColor color : LED_COLORS) {
        const uint16_t c = ledColor565(color);
        // Each channel is all-ones or all-zeros: no grey bit planes (Spec 03 §3).
        for (const uint16_t mask : {uint16_t(0xF800), uint16_t(0x07E0), uint16_t(0x001F)})
            TEST_ASSERT_TRUE((c & mask) == 0 || (c & mask) == mask);
        TEST_ASSERT_TRUE(ledColorChannels(color) >= 1 && ledColorChannels(color) <= ledColorChannels(LedColor::White));
    }
    TEST_ASSERT_EQUAL_UINT(3, ledColorChannels(LedColor::White)); // worst-case current
    TEST_ASSERT_EQUAL_UINT(1, ledColorChannels(LedColor::Red));
}

void test_only_palette_letters_are_color_commands()
{
    int colorCommands = 0;
    for (int a = 0; a < 256; ++a) {
        const char one[] = {static_cast<char>(a)};
        LedColor color = LedColor::White;
        TEST_ASSERT_FALSE(colorCommand(one, 1, color));
        for (int b = 0; b < 256; ++b) {
            const char two[] = {static_cast<char>(a), static_cast<char>(b)};
            if (!colorCommand(two, 2, color)) continue;
            ++colorCommands;
            TEST_ASSERT_EQUAL_CHAR('K', two[0]);
            TEST_ASSERT_EQUAL_CHAR(two[1], static_cast<char>(color));
            TEST_ASSERT_FALSE(isScoringCommand(two, 2));
            TEST_ASSERT_EQUAL_UINT8(0, brightnessCommandLevel(two, 2));
        }
    }
    TEST_ASSERT_EQUAL_INT(6, colorCommands);
}

void test_malformed_color_commands_change_nothing()
{
    GameState state;
    DisplaySettings settings;
    apply("+R", state, settings);
    for (const char* command : {"K", "KK", "Kr", "KM", "KW0", "kR", "K ", "RK", "K,R", "KRR", "B", "KX"}) {
        const auto effect = apply(command, state, settings);
        TEST_ASSERT_FALSE(effect.colorChanged || effect.brightnessChanged || effect.visibleChange || effect.undoStatusChanged);
    }
    TEST_ASSERT_FALSE(apply(std::string("KR\0", 3), state, settings).colorChanged);
    TEST_ASSERT_FALSE(apply("KW", state, settings).colorChanged); // already White: no report
    TEST_ASSERT_TRUE(settings.color() == LedColor::White);
    assertState(state, 0, 1, Side::LEFT, 1, false);
    TEST_ASSERT_EQUAL_UINT(1, state.historyDepth());
}

void test_color_never_touches_score_serve_brightness_or_undo()
{
    GameState state;
    DisplaySettings settings;
    apply("+L", state, settings);
    apply("S", state, settings);
    apply("B3", state, settings);
    const std::string before = serializeManualStatePayload(state);
    const std::size_t depth = state.historyDepth();
    for (const LedColor color : LED_COLORS) {
        const std::string command = std::string("K") + static_cast<char>(color);
        const auto effect = apply(command, state, settings);
        TEST_ASSERT_EQUAL(color != LedColor::White, effect.colorChanged); // White is first and current
        TEST_ASSERT_FALSE(effect.visibleChange || effect.undoStatusChanged || effect.brightnessChanged);
        TEST_ASSERT_TRUE(settings.color() == color);
        TEST_ASSERT_EQUAL_UINT8(3, settings.level());
        TEST_ASSERT_EQUAL_STRING(before.c_str(), serializeManualStatePayload(state).c_str());
        TEST_ASSERT_EQUAL_UINT(depth, state.historyDepth());
    }
    apply("KR", state, settings);
    apply("E", state, settings);
    TEST_ASSERT_TRUE(apply("KY", state, settings).colorChanged); // allowed after End Game
    apply("0S", state, settings);
    while (state.canUndo()) apply("U", state, settings);
    TEST_ASSERT_TRUE(settings.color() == LedColor::Yellow);
    TEST_ASSERT_EQUAL_UINT8(3, settings.level());
    assertState(state, 0, 0, Side::LEFT, 1, false);
}

void test_save_settles_both_settings_and_writes_only_changes()
{
    const uint32_t delay = DisplaySettings::SAVE_DELAY_MS;
    DisplaySettings settings;
    settings.applyColor(LedColor::Red, 1000);
    TEST_ASSERT_TRUE(settings.colorUnsaved());
    TEST_ASSERT_FALSE(settings.levelUnsaved()); // the store skips the untouched level
    settings.applyLevel(2, 2500); // either setting restarts the shared delay
    TEST_ASSERT_FALSE(settings.saveDue(1000 + delay));
    TEST_ASSERT_TRUE(settings.saveDue(2500 + delay));
    settings.saved(true, 2500 + delay);
    TEST_ASSERT_FALSE(settings.colorUnsaved() || settings.levelUnsaved());
    TEST_ASSERT_FALSE(settings.saveDue(UINT32_MAX));
    settings.applyColor(LedColor::Blue, 9000);
    settings.applyColor(LedColor::Red, 9100); // back to the stored color: nothing to write
    TEST_ASSERT_FALSE(settings.saveDue(9100 + delay));
    settings.applyColor(LedColor::Green, 20000);
    settings.saved(false, 20000 + delay); // failed write: retry after a full delay
    TEST_ASSERT_FALSE(settings.saveDue(20000 + 2 * delay - 1));
    TEST_ASSERT_TRUE(settings.saveDue(20000 + 2 * delay));
}

void test_restore_unknown_color_records_as_white()
{
    TEST_ASSERT_TRUE(restoredLedColor(false, 'R') == LedColor::White);
    for (int stored = 0; stored < 256; ++stored) {
        const LedColor color = restoredLedColor(true, uint8_t(stored));
        TEST_ASSERT_TRUE(isLedColorCode(uint8_t(stored)) ? uint8_t(color) == stored : color == LedColor::White);
    }
    DisplaySettings restored(restoredBrightnessLevel(true, 2), restoredLedColor(true, 'C'));
    TEST_ASSERT_TRUE(restored.color() == LedColor::Cyan);
    TEST_ASSERT_FALSE(restored.saveDue(UINT32_MAX)); // restoring is not a change
}

void test_color_report_is_distinct_from_state_and_brightness()
{
    for (const LedColor color : LED_COLORS) {
        const std::string payload = serializeColorPayload(color);
        TEST_ASSERT_EQUAL_UINT(3, payload.size());
        TEST_ASSERT_EQUAL_CHAR('K', payload[0]); // never a digit or 'B'
        TEST_ASSERT_EQUAL_CHAR(static_cast<char>(color), payload[2]);
    }
    TEST_ASSERT_EQUAL_STRING("K,R", serializeColorPayload(LedColor::Red).c_str());
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
    RUN_TEST(test_palette_is_full_channel_with_white_default);
    RUN_TEST(test_only_palette_letters_are_color_commands);
    RUN_TEST(test_malformed_color_commands_change_nothing);
    RUN_TEST(test_color_never_touches_score_serve_brightness_or_undo);
    RUN_TEST(test_save_settles_both_settings_and_writes_only_changes);
    RUN_TEST(test_restore_unknown_color_records_as_white);
    RUN_TEST(test_color_report_is_distinct_from_state_and_brightness);
    return UNITY_END();
}
