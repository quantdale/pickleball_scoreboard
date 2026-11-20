// BLE command parser interface.
// Parses command bytes from the Android app and dispatches to the state machine.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#ifndef BLE_COMMAND_PARSER_H
#define BLE_COMMAND_PARSER_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "game_state.h"

// Parse one BLE command payload and update `state` accordingly.
// Supports:
//   - Single-byte commands per Spec 02 Section 4: 'L', 'R', 'U', 'C', 'E'.
//   - Three-byte reset command per Spec 02 Section 4a: '0' followed by
//     'L'/'R' (starting side) then 'D'/'S' (game mode). Any other length or
//     invalid side/mode byte (including the old two-byte format) is
//     malformed and ignored.
// Returns whether the six-field projection changed. The caller separately
// checks Undo status for extended notifications (Spec 02 §5).
bool isScoringCommand(const char* bytes, std::size_t length);
bool handleBleCommand(const char* bytes, std::size_t length, GameState& state);
inline bool handleBleCommand(const std::string& value, GameState& state) {
    return handleBleCommand(value.data(), value.size(), state);
}

#endif // BLE_COMMAND_PARSER_H
