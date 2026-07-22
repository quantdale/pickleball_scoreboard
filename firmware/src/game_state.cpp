// Game state machine implementation stub.
// Transition logic will be filled in by a follow-up change.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include "game_state.h"

void initGameState(GameState& state, Side startingSide) {
    // TODO: set state to Spec 01 Section 4 initial values.
    (void)startingSide;
    (void)state;
}

void handleRallyWonLeft(GameState& state) {
    // TODO: Spec 01 Section 5 RALLY_WON_LEFT transition.
    (void)state;
}

void handleRallyWonRight(GameState& state) {
    // TODO: Spec 01 Section 5 RALLY_WON_RIGHT transition.
    (void)state;
}

void handleUndo(GameState& state) {
    // TODO: Spec 01 Section 5a single-step undo.
    (void)state;
}

void handleSwitchCourts(GameState& state) {
    // TODO: Spec 01 Section 5 SWITCH_COURTS transition.
    (void)state;
}

void handleEndGame(GameState& state) {
    // TODO: Spec 01 Section 5b END_GAME transition.
    (void)state;
}

void handleReset(GameState& state, Side startingSide) {
    // TODO: Spec 01 Section 5c RESET transition.
    (void)state;
    (void)startingSide;
}
