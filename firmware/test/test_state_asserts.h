// Shared state assertion helpers for the native Unity test suites.
// See docs/specs/01-scoring-state-machine.md for the authoritative state rules.

#ifndef TEST_STATE_ASSERTS_H
#define TEST_STATE_ASSERTS_H

#include <unity.h>
#include "game_state.h"

static inline void assertState(const GameState& state, int left, int right, Side side, int server, bool ended) {
    TEST_ASSERT_EQUAL_INT(left, state.leftScore);
    TEST_ASSERT_EQUAL_INT(right, state.rightScore);
    TEST_ASSERT_TRUE(state.servingSide == side);
    TEST_ASSERT_EQUAL_INT(server, state.serverNumber);
    TEST_ASSERT_EQUAL(ended, state.gameEnded);
}

static inline void assertStateWithMode(const GameState& state, int left, int right, Side side, int server, GameMode mode, bool ended) {
    assertState(state, left, right, side, server, ended);
    TEST_ASSERT_TRUE(state.gameMode == mode);
}

#endif // TEST_STATE_ASSERTS_H
