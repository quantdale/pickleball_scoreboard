// Unit tests for BLE command parsing.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#include <unity.h>
#include "ble_command_parser.h"

static void assertState(const GameState& state, int left, int right, Side side, int server, bool ended) {
    TEST_ASSERT_EQUAL_INT(left, state.leftScore);
    TEST_ASSERT_EQUAL_INT(right, state.rightScore);
    TEST_ASSERT_TRUE(state.servingSide == side);
    TEST_ASSERT_EQUAL_INT(server, state.serverNumber);
    TEST_ASSERT_EQUAL(ended, state.gameEnded);
}

void setUp(void) {}
void tearDown(void) {}

void test_rally_won_left(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_rally_won_right(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("R", state));
    assertState(state, 0, 0, Side::RIGHT, 2, false);
}

void test_undo_after_rally(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);

    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_undo_with_no_previous_state_first_command(void) {
    GameState state;
    initGameState(state, Side::LEFT);

    // Spec 01 Section 5a: pressing UNDO when there is no saved previous state
    // must be a silent no-op, distinct from an unrecognized byte no-op.
    TEST_ASSERT_FALSE(handleBleCommand("U", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_undo_immediately_after_successful_undo(void) {
    GameState state;
    initGameState(state, Side::LEFT);

    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);

    TEST_ASSERT_TRUE(handleBleCommand("U", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);

    // Second undo in a row has no previous state to restore.
    TEST_ASSERT_FALSE(handleBleCommand("U", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_switch_courts(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    // state is (2,0,L,2)
    TEST_ASSERT_TRUE(handleBleCommand("C", state));
    assertState(state, 0, 2, Side::RIGHT, 2, false);
}

void test_end_game(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("E", state));
    assertState(state, 0, 0, Side::LEFT, 2, true);
}

void test_end_game_when_already_ended_is_no_op(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("E", state));
    TEST_ASSERT_FALSE(handleBleCommand("E", state));
    assertState(state, 0, 0, Side::LEFT, 2, true);
}

void test_reset_left(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    TEST_ASSERT_TRUE(handleBleCommand("L", state));

    TEST_ASSERT_TRUE(handleBleCommand("0L", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_reset_right(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));

    TEST_ASSERT_TRUE(handleBleCommand("0R", state));
    assertState(state, 0, 0, Side::RIGHT, 2, false);
}

void test_unrecognized_byte_is_ignored(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_FALSE(handleBleCommand("X", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_unrecognized_byte_after_state_change_is_ignored(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);

    TEST_ASSERT_FALSE(handleBleCommand("X", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_malformed_reset_missing_second_byte(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));

    TEST_ASSERT_FALSE(handleBleCommand("0", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_malformed_reset_invalid_second_byte(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));

    TEST_ASSERT_FALSE(handleBleCommand("0X", state));
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_empty_payload_is_ignored(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_FALSE(handleBleCommand("", state));
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_rally_is_no_op_while_ended(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    TEST_ASSERT_TRUE(handleBleCommand("E", state));

    TEST_ASSERT_FALSE(handleBleCommand("L", state));
    TEST_ASSERT_FALSE(handleBleCommand("R", state));
    assertState(state, 1, 0, Side::LEFT, 2, true);
}

void test_switch_courts_is_no_op_while_ended(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    TEST_ASSERT_TRUE(handleBleCommand("E", state));

    TEST_ASSERT_FALSE(handleBleCommand("C", state));
    assertState(state, 1, 0, Side::LEFT, 2, true);
}

void test_reset_works_while_ended(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    TEST_ASSERT_TRUE(handleBleCommand("L", state));
    TEST_ASSERT_TRUE(handleBleCommand("E", state));

    TEST_ASSERT_TRUE(handleBleCommand("0R", state));
    assertState(state, 0, 0, Side::RIGHT, 2, false);
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_rally_won_left);
    RUN_TEST(test_rally_won_right);
    RUN_TEST(test_undo_after_rally);
    RUN_TEST(test_undo_with_no_previous_state_first_command);
    RUN_TEST(test_undo_immediately_after_successful_undo);
    RUN_TEST(test_switch_courts);
    RUN_TEST(test_end_game);
    RUN_TEST(test_end_game_when_already_ended_is_no_op);
    RUN_TEST(test_reset_left);
    RUN_TEST(test_reset_right);
    RUN_TEST(test_unrecognized_byte_is_ignored);
    RUN_TEST(test_unrecognized_byte_after_state_change_is_ignored);
    RUN_TEST(test_malformed_reset_missing_second_byte);
    RUN_TEST(test_malformed_reset_invalid_second_byte);
    RUN_TEST(test_empty_payload_is_ignored);
    RUN_TEST(test_rally_is_no_op_while_ended);
    RUN_TEST(test_switch_courts_is_no_op_while_ended);
    RUN_TEST(test_reset_works_while_ended);

    UNITY_END();
}

void loop() {
    // Empty; tests run once in setup().
}
