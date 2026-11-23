// Unit tests for the manual scoring state.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include <unity.h>
#include <vector>
#include "game_state.h"
#include "../test_state_asserts.h"

void setUp(void) {}
void tearDown(void) {}

void test_initial_state_is_left_server_one_doubles(void) {
    GameState state;
    initGameState(state);
    assertStateWithMode(state, 0, 0, Side::LEFT, 1, GameMode::DOUBLES, false);
    TEST_ASSERT_FALSE(state.canUndo());
}

void test_direct_adjustments_change_only_their_score(void) {
    GameState state;
    for (int i = 0; i < 7; ++i) handleScoreUp(state, Side::LEFT);
    for (int i = 0; i < 5; ++i) handleScoreUp(state, Side::RIGHT);
    assertState(state, 7, 5, Side::LEFT, 1, false);
    handleScoreDown(state, Side::RIGHT);
    handleScoreDown(state, Side::LEFT);
    assertState(state, 6, 4, Side::LEFT, 1, false); // serving never changes with scores
    TEST_ASSERT_EQUAL_UINT(14, state.historyDepth());
}

void test_scores_clamp_at_zero_and_ninety_nine_without_history(void) {
    GameState state;
    handleScoreDown(state, Side::LEFT);
    handleScoreDown(state, Side::RIGHT);
    TEST_ASSERT_FALSE(state.canUndo());
    for (int i = 0; i < 150; ++i) handleScoreUp(state, Side::RIGHT);
    TEST_ASSERT_EQUAL_INT(99, state.rightScore);
    TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_POSITIONS < 99 ? GameState::HISTORY_POSITIONS : 99, state.historyDepth());
    handleUndo(state);
    TEST_ASSERT_EQUAL_INT(98, state.rightScore);
}

void test_doubles_serve_cycles_exactly_four_states(void) {
    GameState state;
    const Side sides[] = {Side::LEFT, Side::RIGHT, Side::RIGHT, Side::LEFT, Side::LEFT};
    const int servers[] = {2, 1, 2, 1, 2};
    for (int i = 0; i < 5; ++i) {
        handleNextServe(state);
        assertState(state, 0, 0, sides[i], servers[i], false);
    }
}

void test_singles_serve_alternates_with_one_arrow(void) {
    GameState state;
    handleReset(state, GameMode::SINGLES);
    handleNextServe(state);
    assertStateWithMode(state, 0, 0, Side::RIGHT, 1, GameMode::SINGLES, false);
    handleNextServe(state);
    assertStateWithMode(state, 0, 0, Side::LEFT, 1, GameMode::SINGLES, false);
}

void test_switch_courts_swaps_scores_and_flips_serving_end(void) {
    GameState state;
    handleScoreUp(state, Side::LEFT);
    handleNextServe(state); // L2
    handleSwitchCourts(state);
    assertState(state, 0, 1, Side::RIGHT, 2, false);
    handleSwitchCourts(state);
    assertState(state, 1, 0, Side::LEFT, 2, false);
}

void test_end_game_freezes_play_inputs_and_undo_resumes(void) {
    GameState state;
    handleScoreUp(state, Side::LEFT);
    handleEndGame(state);
    const std::size_t depth = state.historyDepth();
    handleScoreUp(state, Side::LEFT);
    handleScoreDown(state, Side::LEFT);
    handleNextServe(state);
    handleSwitchCourts(state);
    handleEndGame(state);
    assertState(state, 1, 0, Side::LEFT, 1, true);
    TEST_ASSERT_EQUAL_UINT(depth, state.historyDepth());
    handleUndo(state);
    assertState(state, 1, 0, Side::LEFT, 1, false);
    handleScoreUp(state, Side::LEFT);
    TEST_ASSERT_EQUAL_INT(2, state.leftScore);
}

void test_reset_starts_new_game_while_ended_and_is_undoable(void) {
    GameState state;
    handleScoreUp(state, Side::RIGHT);
    handleNextServe(state);
    handleNextServe(state);
    handleEndGame(state);
    handleReset(state, GameMode::SINGLES);
    assertStateWithMode(state, 0, 0, Side::LEFT, 1, GameMode::SINGLES, false);
    handleUndo(state);
    assertStateWithMode(state, 0, 1, Side::RIGHT, 1, GameMode::DOUBLES, true);
}

void test_same_position_reset_is_recorded(void) {
    GameState state;
    handleReset(state, GameMode::DOUBLES);
    TEST_ASSERT_TRUE(state.canUndo());
    handleUndo(state);
    TEST_ASSERT_FALSE(state.canUndo());
    handleUndo(state); // empty history is a no-op
    assertState(state, 0, 0, Side::LEFT, 1, false);
}

void test_worked_example_from_spec(void) {
    GameState state;
    for (int i = 0; i < 7; ++i) handleScoreUp(state, Side::LEFT);
    for (int i = 0; i < 5; ++i) handleScoreUp(state, Side::RIGHT);
    handleNextServe(state);
    assertState(state, 7, 5, Side::LEFT, 2, false);
    handleNextServe(state);
    assertState(state, 7, 5, Side::RIGHT, 1, false);
    handleScoreDown(state, Side::RIGHT);
    handleSwitchCourts(state);
    assertState(state, 4, 7, Side::LEFT, 1, false);
    handleEndGame(state);
    handleScoreUp(state, Side::LEFT);
    assertState(state, 4, 7, Side::LEFT, 1, true);
    handleUndo(state);
    assertState(state, 4, 7, Side::LEFT, 1, false);
    handleReset(state, GameMode::SINGLES);
    handleNextServe(state);
    assertStateWithMode(state, 0, 0, Side::RIGHT, 1, GameMode::SINGLES, false);
}

void test_undo_restores_every_field_of_every_prior_position(void) {
    GameState state;
    std::vector<GamePosition> timeline;
    uint32_t rng = 0x5eedu;
    for (int i = 0; i < 1500; ++i) {
        rng = rng * 1664525u + 1013904223u;
        const GamePosition before = state;
        const std::size_t depth = state.historyDepth();
        switch ((rng >> 24) % 9) {
            case 0: handleScoreUp(state, Side::LEFT); break;
            case 1: handleScoreUp(state, Side::RIGHT); break;
            case 2: handleScoreDown(state, Side::LEFT); break;
            case 3: handleScoreDown(state, Side::RIGHT); break;
            case 4: handleNextServe(state); break;
            case 5: handleSwitchCourts(state); break;
            case 6: handleEndGame(state); break;
            case 7: handleReset(state, (rng & 1) ? GameMode::SINGLES : GameMode::DOUBLES); break;
            default: handleScoreUp(state, Side::LEFT); break;
        }
        if (state.historyDepth() != depth) timeline.push_back(before);
        else TEST_ASSERT_TRUE(state.sameVisibleState(before));
        // Unwind each full window before the ring would drop a record, then play on.
        if (state.historyDepth() == GameState::HISTORY_POSITIONS || i == 1499) {
            for (auto it = timeline.rbegin(); it != timeline.rend(); ++it) {
                handleUndo(state);
                TEST_ASSERT_TRUE(state.sameVisibleState(*it));
            }
            TEST_ASSERT_FALSE(state.canUndo());
            timeline.clear();
        }
    }
}

void test_full_ring_drops_oldest_and_never_refuses_input(void) {
    GameState state;
    const std::size_t total = GameState::HISTORY_POSITIONS + 300;
    for (std::size_t i = 0; i < total; ++i) {
        if (i % 2) {
            handleNextServe(state);
        } else if (state.leftScore < 99) {
            handleScoreUp(state, Side::LEFT);
        } else {
            handleScoreDown(state, Side::RIGHT);
            handleScoreUp(state, Side::RIGHT);
        }
    }
    TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_POSITIONS, state.historyDepth());
    TEST_ASSERT_EQUAL_UINT(GameState::HISTORY_BYTES, state.historyBytesUsed());
    // Every one of the retained positions unwinds; only the oldest 300+ are gone.
    for (std::size_t i = 0; i < GameState::HISTORY_POSITIONS; ++i) {
        TEST_ASSERT_TRUE(state.canUndo());
        handleUndo(state);
    }
    TEST_ASSERT_FALSE(state.canUndo());
    TEST_ASSERT_TRUE(state.leftScore > 0); // not the boot position: it was evicted
    handleScoreUp(state, Side::RIGHT);
    TEST_ASSERT_EQUAL_UINT(1, state.historyDepth());
}

void test_init_discards_history(void) {
    GameState state;
    handleScoreUp(state, Side::LEFT);
    initGameState(state, GameMode::SINGLES);
    assertStateWithMode(state, 0, 0, Side::LEFT, 1, GameMode::SINGLES, false);
    TEST_ASSERT_FALSE(state.canUndo());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_state_is_left_server_one_doubles);
    RUN_TEST(test_direct_adjustments_change_only_their_score);
    RUN_TEST(test_scores_clamp_at_zero_and_ninety_nine_without_history);
    RUN_TEST(test_doubles_serve_cycles_exactly_four_states);
    RUN_TEST(test_singles_serve_alternates_with_one_arrow);
    RUN_TEST(test_switch_courts_swaps_scores_and_flips_serving_end);
    RUN_TEST(test_end_game_freezes_play_inputs_and_undo_resumes);
    RUN_TEST(test_reset_starts_new_game_while_ended_and_is_undoable);
    RUN_TEST(test_same_position_reset_is_recorded);
    RUN_TEST(test_worked_example_from_spec);
    RUN_TEST(test_undo_restores_every_field_of_every_prior_position);
    RUN_TEST(test_full_ring_drops_oldest_and_never_refuses_input);
    RUN_TEST(test_init_discards_history);
    return UNITY_END();
}
