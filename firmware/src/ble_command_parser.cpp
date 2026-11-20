// BLE command parser implementation.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#include "ble_command_parser.h"

bool isScoringCommand(const char* bytes, std::size_t length) {
    // Spec 02 §§4, 4a, 7: exact length and values, no partial writes.
    if (length == 1) {
        const char c = bytes[0];
        return c == 'L' || c == 'R' || c == 'U' || c == 'C' || c == 'E';
    }
    return length == 3 && bytes[0] == '0'
        && (bytes[1] == 'L' || bytes[1] == 'R') && (bytes[2] == 'D' || bytes[2] == 'S');
}

bool handleBleCommand(const char* bytes, std::size_t length, GameState& state) {
    if (!isScoringCommand(bytes, length)) return false;

    const GamePosition before = state;
    switch (bytes[0]) {
        case 'L': handleRallyWonLeft(state); break;
        case 'R': handleRallyWonRight(state); break;
        case 'U': handleUndo(state); break;
        case 'C': handleSwitchCourts(state); break;
        case 'E': handleEndGame(state); break;
        case '0': handleReset(state, bytes[1] == 'L' ? Side::LEFT : Side::RIGHT,
                              bytes[2] == 'S' ? GameMode::SINGLES : GameMode::DOUBLES); break;
    }
    return !state.sameVisibleState(before);
}
