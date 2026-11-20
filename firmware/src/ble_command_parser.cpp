// BLE command parser implementation.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#include "ble_command_parser.h"

bool isScoringCommand(const char* bytes, std::size_t length) {
    // Spec 02 §§4, 7: exact length and values, no partial writes.
    if (length == 1) {
        const char c = bytes[0];
        return c == 'S' || c == 'C' || c == 'E' || c == 'U';
    }
    if (length != 2) return false;
    if (bytes[0] == '+' || bytes[0] == '-') return bytes[1] == 'L' || bytes[1] == 'R';
    return bytes[0] == '0' && (bytes[1] == 'D' || bytes[1] == 'S');
}

bool handleBleCommand(const char* bytes, std::size_t length, GameState& state) {
    if (!isScoringCommand(bytes, length)) return false;

    const GamePosition before = state;
    const Side side = length == 2 && bytes[1] == 'R' ? Side::RIGHT : Side::LEFT;
    switch (bytes[0]) {
        case '+': handleScoreUp(state, side); break;
        case '-': handleScoreDown(state, side); break;
        case 'S': handleNextServe(state); break;
        case 'C': handleSwitchCourts(state); break;
        case 'E': handleEndGame(state); break;
        case 'U': handleUndo(state); break;
        case '0': handleReset(state, bytes[1] == 'S' ? GameMode::SINGLES : GameMode::DOUBLES); break;
    }
    return !state.sameVisibleState(before);
}
