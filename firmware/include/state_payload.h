#ifndef STATE_PAYLOAD_H
#define STATE_PAYLOAD_H

#include <cstdio>
#include <string>
#include "game_state.h"

// Spec 02 §5: six Legacy State fields, or those fields plus Undo availability.
inline std::string serializeStatePayload(const GameState& state)
{
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%d,%d,%c,%d,%c,%c", state.leftScore,
                  state.rightScore, state.servingSide == Side::LEFT ? 'L' : 'R',
                  state.serverNumber, state.gameEnded ? '1' : '0',
                  state.gameMode == GameMode::SINGLES ? 'S' : 'D');
    return buffer;
}

inline std::string serializeManualStatePayload(const GameState& state)
{
    std::string payload = serializeStatePayload(state);
    payload += ',';
    payload += static_cast<char>('0' + state.undoStatus());
    return payload;
}

#endif
