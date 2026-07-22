// Game state machine implementation.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include "game_state.h"

// Single-step undo storage (Spec 01 Section 5a).
static GameState previousState;
static bool hasPreviousState = false;

static void saveSnapshot(const GameState& state) {
    previousState = state;
    hasPreviousState = true;
}

static void incrementScore(GameState& state, Side side) {
    if (side == Side::LEFT) {
        ++state.leftScore;
    } else {
        ++state.rightScore;
    }
}

static Side otherSide(Side side) {
    return (side == Side::LEFT) ? Side::RIGHT : Side::LEFT;
}

void initGameState(GameState& state, Side startingSide) {
    state.leftScore = 0;
    state.rightScore = 0;
    state.servingSide = startingSide;
    state.serverNumber = 2;
    state.gameMode = GameMode::DOUBLES;
    state.gameEnded = false;
}

static void handleRallyWonBy(GameState& state, Side winningSide) {
    if (state.gameEnded) {
        return;
    }

    saveSnapshot(state);

    if (winningSide == state.servingSide) {
        // Case A: serving side won their own rally.
        incrementScore(state, winningSide);
    } else {
        // Case B: non-serving side won — fault/side-out.
        if (state.serverNumber == 1) {
            // B1: move to second server on the same side.
            state.serverNumber = 2;
        } else {
            // B2: full side-out to the winning side, starting at 0-0-2.
            state.servingSide = winningSide;
            state.serverNumber = 2;
        }
    }
}

void handleRallyWonLeft(GameState& state) {
    handleRallyWonBy(state, Side::LEFT);
}

void handleRallyWonRight(GameState& state) {
    handleRallyWonBy(state, Side::RIGHT);
}

void handleUndo(GameState& state) {
    // Spec 01 Section 5a (amended): UNDO applies uniformly, including
    // immediately after END_GAME and RESET.
    if (hasPreviousState) {
        state = previousState;
        hasPreviousState = false;
    }
}

void handleSwitchCourts(GameState& state) {
    if (state.gameEnded) {
        return;
    }

    saveSnapshot(state);

    int tmp = state.leftScore;
    state.leftScore = state.rightScore;
    state.rightScore = tmp;

    state.servingSide = otherSide(state.servingSide);
}

void handleEndGame(GameState& state) {
    if (state.gameEnded) {
        return;
    }

    saveSnapshot(state);
    state.gameEnded = true;
}

void handleReset(GameState& state, Side startingSide) {
    saveSnapshot(state);
    initGameState(state, startingSide);
}
