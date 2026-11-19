// Game state machine interface.
// See docs/specs/01-scoring-state-machine.md for the authoritative rules.

#ifndef GAME_STATE_H
#define GAME_STATE_H

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

struct GameState : GamePosition {
    // Spec 01 §5a: one snapshot per game, without recursively copying history.
    GamePosition previous;
    bool canUndo = false;
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
