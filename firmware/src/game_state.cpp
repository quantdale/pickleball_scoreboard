// Manual scoring state implementation.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#include "game_state.h"

namespace {
constexpr uint8_t RIGHT_SERVING = 1;
constexpr uint8_t SERVER_TWO = 2;
constexpr uint8_t SINGLES = 4;
constexpr uint8_t ENDED = 8;
}

void GameState::clearHistory() {
    next = 0;
    depth = 0;
}

// Spec 01 §6: one fixed-size record per accepted input: both scores, then flags.
void GameState::record() {
    uint8_t* slot = history + next * RECORD_BYTES;
    slot[0] = static_cast<uint8_t>(leftScore);
    slot[1] = static_cast<uint8_t>(rightScore);
    slot[2] = (servingSide == Side::RIGHT ? RIGHT_SERVING : 0) | (serverNumber == 2 ? SERVER_TWO : 0)
        | (gameMode == GameMode::SINGLES ? SINGLES : 0) | (gameEnded ? ENDED : 0);
    next = static_cast<uint16_t>((next + 1) % HISTORY_POSITIONS);
    if (depth < HISTORY_POSITIONS) ++depth; // otherwise the oldest record was overwritten
}

void GameState::undo() {
    if (!canUndo()) return;
    next = static_cast<uint16_t>((next + HISTORY_POSITIONS - 1) % HISTORY_POSITIONS);
    --depth;
    const uint8_t* slot = history + next * RECORD_BYTES;
    leftScore = slot[0];
    rightScore = slot[1];
    servingSide = slot[2] & RIGHT_SERVING ? Side::RIGHT : Side::LEFT;
    serverNumber = slot[2] & SERVER_TWO ? 2 : 1;
    gameMode = slot[2] & SINGLES ? GameMode::SINGLES : GameMode::DOUBLES;
    gameEnded = (slot[2] & ENDED) != 0;
}

static GamePosition newGame(GameMode mode) {
    GamePosition position;
    position.gameMode = mode;
    return position;
}

void initGameState(GameState& state, GameMode mode) {
    static_cast<GamePosition&>(state) = newGame(mode);
    state.clearHistory();
}

static int& scoreAt(GameState& state, Side side) {
    return side == Side::LEFT ? state.leftScore : state.rightScore;
}

// Spec 01 §3: direct corrections, clamped to 0..99; a bound or ended game is ignored.
void handleScoreUp(GameState& state, Side side) {
    if (state.gameEnded || scoreAt(state, side) >= GamePosition::MAX_SCORE) return;
    state.record();
    ++scoreAt(state, side);
}

void handleScoreDown(GameState& state, Side side) {
    if (state.gameEnded || scoreAt(state, side) <= 0) return;
    state.record();
    --scoreAt(state, side);
}

// Spec 01 §5: Doubles L1 -> L2 -> R1 -> R2 -> L1; Singles L1 <-> R1.
void handleNextServe(GameState& state) {
    if (state.gameEnded) return;
    state.record();
    if (state.gameMode == GameMode::DOUBLES && state.serverNumber == 1) {
        state.serverNumber = 2;
        return;
    }
    state.serverNumber = 1;
    state.servingSide = state.servingSide == Side::LEFT ? Side::RIGHT : Side::LEFT;
}

void handleSwitchCourts(GameState& state) {
    if (state.gameEnded) return;
    state.record();
    const int left = state.leftScore;
    state.leftScore = state.rightScore;
    state.rightScore = left;
    state.servingSide = state.servingSide == Side::LEFT ? Side::RIGHT : Side::LEFT;
}

void handleEndGame(GameState& state) {
    if (state.gameEnded) return;
    state.record();
    state.gameEnded = true;
}

void handleUndo(GameState& state) {
    state.undo(); // Spec 01 §6: also resumes an ended game and reverts Reset.
}

void handleReset(GameState& state, GameMode mode) {
    state.record(); // Spec 01 §4: recorded even when the position is unchanged.
    static_cast<GamePosition&>(state) = newGame(mode);
}
