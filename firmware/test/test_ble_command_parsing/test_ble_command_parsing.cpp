// Unit tests for BLE command parsing.
// See docs/specs/02-ble-protocol.md Section 4 for the command set.

#include <unity.h>
#include <string>
#include "ble_command_parser.h"
#include "../test_state_asserts.h"

void setUp(void) {}
void tearDown(void) {}

void test_score_commands_adjust_each_side(void) {
    GameState state;
    TEST_ASSERT_TRUE(handleBleCommand("+L", state));
    TEST_ASSERT_TRUE(handleBleCommand("+R", state));
    TEST_ASSERT_TRUE(handleBleCommand("+R", state));
    assertState(state, 1, 2, Side::LEFT, 1, false);
    TEST_ASSERT_TRUE(handleBleCommand("-R", state));
    TEST_ASSERT_TRUE(handleBleCommand("-L", state));
    assertState(state, 0, 1, Side::LEFT, 1, false);
    TEST_ASSERT_FALSE(handleBleCommand("-L", state)); // clamped at zero
}

void test_serve_switch_end_and_undo(void) {
    GameState state;
    TEST_ASSERT_TRUE(handleBleCommand("S", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
    TEST_ASSERT_TRUE(handleBleCommand("+L", state));
    TEST_ASSERT_TRUE(handleBleCommand("C", state));
    assertState(state, 0, 1, Side::RIGHT, 2, false);
    TEST_ASSERT_TRUE(handleBleCommand("E", state));
    TEST_ASSERT_FALSE(handleBleCommand("E", state));
    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    assertState(state, 0, 1, Side::RIGHT, 2, false);
}

void test_reset_selects_mode(void) {
    GameState state;
    handleBleCommand("+L", state);
    handleBleCommand("S", state);
    TEST_ASSERT_TRUE(handleBleCommand("0S", state));
    assertStateWithMode(state, 0, 0, Side::LEFT, 1, GameMode::SINGLES, false);
    TEST_ASSERT_TRUE(handleBleCommand("0D", state));
    assertStateWithMode(state, 0, 0, Side::LEFT, 1, GameMode::DOUBLES, false);
}

void test_exactly_ten_commands_are_accepted(void) {
    int accepted = 0;
    for (int a = 0; a < 256; ++a) {
        const char one[] = {static_cast<char>(a)};
        accepted += isScoringCommand(one, 1);
        for (int b = 0; b < 256; ++b) {
            const char two[] = {static_cast<char>(a), static_cast<char>(b)};
            accepted += isScoringCommand(two, 2);
        }
    }
    TEST_ASSERT_EQUAL_INT(10, accepted);
}

void test_portrait_commands_are_malformed(void) {
    GameState state;
    handleBleCommand("+L", state);
    for (const char* command : {"L", "R", "0", "0L", "0R", "0LD", "0RS", "0LS", "0RD"}) {
        TEST_ASSERT_FALSE(isScoringCommand(command, std::string(command).size()));
        TEST_ASSERT_FALSE(handleBleCommand(command, state));
    }
    assertState(state, 1, 0, Side::LEFT, 1, false);
    TEST_ASSERT_EQUAL_UINT(1, state.historyDepth());
}

void test_wrong_lengths_and_empty_payload_are_ignored(void) {
    GameState state;
    TEST_ASSERT_FALSE(handleBleCommand("", state));
    TEST_ASSERT_FALSE(handleBleCommand(std::string("+L\0", 3), state));
    TEST_ASSERT_FALSE(handleBleCommand("+L+", state));
    TEST_ASSERT_FALSE(handleBleCommand("SS", state));
    TEST_ASSERT_FALSE(handleBleCommand("++", state));
    TEST_ASSERT_FALSE(handleBleCommand("0X", state));
    TEST_ASSERT_FALSE(handleBleCommand("+l", state));
    TEST_ASSERT_FALSE(handleBleCommand("s", state));
    assertState(state, 0, 0, Side::LEFT, 1, false);
    TEST_ASSERT_FALSE(state.canUndo());
}

void test_ignored_inputs_while_ended_report_no_change(void) {
    GameState state;
    handleBleCommand("E", state);
    for (const char* command : {"+L", "-L", "+R", "-R", "S", "C", "E"})
        TEST_ASSERT_FALSE(handleBleCommand(command, state));
    TEST_ASSERT_EQUAL_UINT(1, state.historyDepth());
    TEST_ASSERT_TRUE(handleBleCommand("0D", state));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_score_commands_adjust_each_side);
    RUN_TEST(test_serve_switch_end_and_undo);
    RUN_TEST(test_reset_selects_mode);
    RUN_TEST(test_exactly_ten_commands_are_accepted);
    RUN_TEST(test_portrait_commands_are_malformed);
    RUN_TEST(test_wrong_lengths_and_empty_payload_are_ignored);
    RUN_TEST(test_ignored_inputs_while_ended_report_no_change);
    return UNITY_END();
}
