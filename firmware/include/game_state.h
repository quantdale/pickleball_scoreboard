// Game state machine interface.
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

struct GamePosition {
    int leftScore = 0;
    int rightScore = 0;
    Side servingSide = Side::LEFT;
    int serverNumber = 2;
    GameMode gameMode = GameMode::DOUBLES;
    bool gameEnded = false;
    bool openingService = true; // Spec 01 §4: internal; not part of the BLE projection.

    bool sameVisibleState(const GamePosition& other) const {
        return leftScore == other.leftScore
            && rightScore == other.rightScore
            && servingSide == other.servingSide
            && serverNumber == other.serverNumber
            && gameMode == other.gameMode
            && gameEnded == other.gameEnded;
    }

};

// Spec 01 §5a: loop-owned journal, not copied with a visible position.
class GameState : public GamePosition {
public:
    static constexpr std::size_t HISTORY_BYTES = 6144;
    GameState() = default;
    GameState(const GameState&) = delete;
    GameState& operator=(const GameState&) = delete;

    enum class Change : uint8_t { Flags, Point, Switch, Reset };
    bool record(Change change);
    void undo();
    void clearHistory();
    bool canUndo() const { return used != 0; }
    unsigned undoStatus() const { return (canUndo() ? 1u : 0u) | (blocked ? 2u : 0u); }
    std::size_t historyBytesUsed() const { return used; }

private:
    static_assert(HISTORY_BYTES <= UINT16_MAX, "Undo cursor must fit");
    uint8_t history[HISTORY_BYTES];
    uint16_t used = 0;
    bool blocked = false;
};

// Initialize a new game with the selected 0-0-2 side and game mode
// (Spec 01 Section 4, Section 9b).
void initGameState(GameState& state, Side startingSide, GameMode mode = GameMode::DOUBLES);

// Spec 01 Section 5 inputs.
void handleRallyWonLeft(GameState& state);
void handleRallyWonRight(GameState& state);
void handleUndo(GameState& state);
void handleSwitchCourts(GameState& state);
void handleEndGame(GameState& state);

// Reset requires the starting 0-0-2 side and game mode (Spec 02 Section 4a).
void handleReset(GameState& state, Side startingSide, GameMode mode = GameMode::DOUBLES);

#endif // GAME_STATE_H
