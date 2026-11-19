// Game state machine implementation.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include "game_state.h"
#include <limits>

static void saveSnapshot(GameState& state) {
    state.previous = state;
    state.canUndo = true;
}

static Side otherSide(Side side) {
    return (side == Side::LEFT) ? Side::RIGHT : Side::LEFT;
}

static GamePosition openingPosition(Side startingSide, GameMode mode) {
    GamePosition position;
    position.servingSide = startingSide;
    position.gameMode = mode;
    position.openingService = mode == GameMode::DOUBLES;
    return position;
}

void initGameState(GameState& state, Side startingSide, GameMode mode) {
    static_cast<GamePosition&>(state) = openingPosition(startingSide, mode);
    state.canUndo = false;
}

static void handleRallyWonBy(GameState& state, Side winningSide) {
    if (state.gameEnded) {
        return;
    }

    if (winningSide == state.servingSide) {
        // Spec 01 §5 A: only the serving side scores; a saturated counter is a no-op.
        int& score = winningSide == Side::LEFT ? state.leftScore : state.rightScore;
        if (score == std::numeric_limits<int>::max()) return;
        saveSnapshot(state);
        ++score;
        return;
    }

    saveSnapshot(state);
    const bool doubles = state.gameMode == GameMode::DOUBLES;
    if (doubles && !state.openingService && state.serverNumber == 1) {
        // Spec 01 §5 B1: partner takes over on the same team; no point.
        state.serverNumber = 2;
        return;
    }
    // Spec 01 §5 B2 and §9a: after a side-out, normal doubles starts on 1.
    state.openingService = false;
    state.servingSide = winningSide;
    state.serverNumber = doubles ? 1 : 2;
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
    if (state.canUndo) {
        static_cast<GamePosition&>(state) = state.previous;
        state.canUndo = false;
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

void handleReset(GameState& state, Side startingSide, GameMode mode) {
    saveSnapshot(state);
    static_cast<GamePosition&>(state) = openingPosition(startingSide, mode);
}
