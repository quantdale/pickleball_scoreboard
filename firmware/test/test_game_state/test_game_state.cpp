// Unit tests for the scoring state machine.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include <unity.h>
#include "game_state.h"

static void assertState(const GameState& state, int left, int right, Side side, int server, bool ended) {
    TEST_ASSERT_EQUAL_INT(left, state.leftScore);
    TEST_ASSERT_EQUAL_INT(right, state.rightScore);
    TEST_ASSERT_TRUE(state.servingSide == side);
    TEST_ASSERT_EQUAL_INT(server, state.serverNumber);
    TEST_ASSERT_EQUAL(ended, state.gameEnded);
}

void setUp(void) {}
void tearDown(void) {}

void test_initial_state(void) {
    GameState state;
    initGameState(state, Side::RIGHT);
    assertState(state, 0, 0, Side::RIGHT, 2, false);
}

void test_worked_example_from_spec(void) {
    GameState state;
    initGameState(state, Side::LEFT);

    // Step 1: L serving, L won -> score up.
    handleRallyWonLeft(state);
    assertState(state, 1, 0, Side::LEFT, 2, false);

    // Step 2: L serving (2nd), R won -> side-out, serve to R at server 2.
    handleRallyWonRight(state);
    assertState(state, 1, 0, Side::RIGHT, 2, false);

    // Step 3: R serving (2nd), L won -> side-out, serve to L at server 2.
    handleRallyWonLeft(state);
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_case_a_serving_side_wins(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_case_b1_server_number_one_fault(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    state.serverNumber = 1; // force first-server state

    handleRallyWonRight(state); // non-serving side wins
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_case_b2_full_side_out(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonRight(state);
    // Spec 01 Section 5 Case B2: no score changes on a side-out.
    assertState(state, 0, 0, Side::RIGHT, 2, false);
}

void test_switch_courts(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    handleRallyWonLeft(state);
    // state is (2,0,L,2)
    handleSwitchCourts(state);
    assertState(state, 0, 2, Side::RIGHT, 2, false);
}

void test_undo_single_step(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state); // (1,0,L,2)
    handleUndo(state);
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_undo_double_does_nothing(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    handleUndo(state);
    handleUndo(state);
    assertState(state, 0, 0, Side::LEFT, 2, false);
}

void test_undo_after_end_game(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);   // (1,0,L,2)
    handleRallyWonRight(state);  // (1,0,R,2)
    handleEndGame(state);        // freeze

    assertState(state, 1, 0, Side::RIGHT, 2, true);

    handleUndo(state);
    assertState(state, 1, 0, Side::RIGHT, 2, false);
}

void test_undo_after_reset(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    handleRallyWonLeft(state);
    // state is (2,0,L,2)

    handleReset(state, Side::RIGHT);
    assertState(state, 0, 0, Side::RIGHT, 2, false);

    handleUndo(state);
    assertState(state, 2, 0, Side::LEFT, 2, false);
}

void test_end_game_freezes_rally_and_switch_inputs(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    handleEndGame(state);

    // Rallies and court switches are no-ops while ended; undo is explicitly
    // allowed by the amended Spec 01 Section 5a and is tested separately.
    handleRallyWonLeft(state);
    handleRallyWonRight(state);
    handleSwitchCourts(state);

    assertState(state, 1, 0, Side::LEFT, 2, true);
}

void test_reset_recovers_from_ended_state(void) {
    GameState state;
    initGameState(state, Side::LEFT);
    handleRallyWonLeft(state);
    handleEndGame(state);

    handleReset(state, Side::RIGHT);
    assertState(state, 0, 0, Side::RIGHT, 2, false);
}

void setup() {
    UNITY_BEGIN();

    RUN_TEST(test_initial_state);
    RUN_TEST(test_worked_example_from_spec);
    RUN_TEST(test_case_a_serving_side_wins);
    RUN_TEST(test_case_b1_server_number_one_fault);
    RUN_TEST(test_case_b2_full_side_out);
    RUN_TEST(test_switch_courts);
    RUN_TEST(test_undo_single_step);
    RUN_TEST(test_undo_double_does_nothing);
    RUN_TEST(test_undo_after_end_game);
    RUN_TEST(test_undo_after_reset);
    RUN_TEST(test_end_game_freezes_rally_and_switch_inputs);
    RUN_TEST(test_reset_recovers_from_ended_state);

    UNITY_END();
}

void loop() {
    // Empty; tests run once in setup().
}
