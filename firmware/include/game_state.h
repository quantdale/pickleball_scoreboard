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

struct GameState {
    int leftScore = 0;
    int rightScore = 0;
    Side servingSide = Side::LEFT;
    int serverNumber = 2;
    GameMode gameMode = GameMode::DOUBLES;
    bool gameEnded = false;
};

// Initialize a new game with the selected 0-0-2 side (Spec 01 Section 4).
void initGameState(GameState& state, Side startingSide);

// Spec 01 Section 5 inputs.
void handleRallyWonLeft(GameState& state);
void handleRallyWonRight(GameState& state);
void handleUndo(GameState& state);
void handleSwitchCourts(GameState& state);
void handleEndGame(GameState& state);

// Reset requires the starting 0-0-2 side parameter (Spec 02 Section 4a).
void handleReset(GameState& state, Side startingSide);

#endif // GAME_STATE_H
