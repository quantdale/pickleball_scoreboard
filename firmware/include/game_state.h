// Game state interface.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <cstddef>
#include <cstdint>

enum class Side {
    LEFT,
    RIGHT
};

enum class GameMode {
    SINGLES,
    DOUBLES
};

// Spec 01 §2: every field is visible on the BLE projection.
struct GamePosition {
    static constexpr int MAX_SCORE = 99;

    int leftScore = 0;
    int rightScore = 0;
    Side servingSide = Side::LEFT;
    int serverNumber = 1;
    GameMode gameMode = GameMode::DOUBLES;
    bool gameEnded = false;

    bool sameVisibleState(const GamePosition& other) const {
        return leftScore == other.leftScore
            && rightScore == other.rightScore
            && servingSide == other.servingSide
            && serverNumber == other.serverNumber
            && gameMode == other.gameMode
            && gameEnded == other.gameEnded;
    }
};

// Spec 01 §6: loop-owned ring of recent prior positions, never heap-allocated.
// The controller has no Undo key; the ring serves Resume after End Game and older
// controllers. Every static byte here is a byte of OTA heap headroom.
class GameState : public GamePosition {
public:
    static constexpr std::size_t RECORD_BYTES = 3;
    static constexpr std::size_t HISTORY_POSITIONS = 32;
    static constexpr std::size_t HISTORY_BYTES = HISTORY_POSITIONS * RECORD_BYTES;
    GameState() = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    // Save the current position before an accepted input; drops the oldest when full.
    void record();
    void undo();
    void clearHistory();
    bool canUndo() const { return depth != 0; }
    unsigned undoStatus() const { return canUndo() ? 1u : 0u; }
    std::size_t historyDepth() const { return depth; }
    std::size_t historyBytesUsed() const { return depth * RECORD_BYTES; }

private:
    static_assert(MAX_SCORE <= UINT8_MAX, "A record stores each score in one byte");
    static_assert(HISTORY_POSITIONS <= UINT16_MAX, "Ring cursors must fit");
    uint8_t history[HISTORY_BYTES];
    uint16_t next = 0;
    uint16_t depth = 0;
};

// Spec 01 §4: 0-0, Left serving, server 1, live, empty history.
void initGameState(GameState& state, GameMode mode = GameMode::DOUBLES);

// Spec 01 §3 inputs.
void handleScoreUp(GameState& state, Side side);
void handleScoreDown(GameState& state, Side side);
void handleNextServe(GameState& state);
void handleSwitchCourts(GameState& state);
void handleEndGame(GameState& state);
void handleUndo(GameState& state);
void handleReset(GameState& state, GameMode mode);

#endif // GAME_STATE_H
