#ifndef STATE_PAYLOAD_H
#define STATE_PAYLOAD_H

#include <cstdio>
#include <string>
#include "game_state.h"

// Spec 02 Section 5: shared, host-testable serialization of all six fields.
inline std::string serializeStatePayload(const GameState& state)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d,%d,%c,%d,%c,%c", state.leftScore,
                  state.rightScore, state.servingSide == Side::LEFT ? 'L' : 'R',
                  state.serverNumber, state.gameEnded ? '1' : '0',
                  state.gameMode == GameMode::SINGLES ? 'S' : 'D');
    return buffer;
}

#endif
