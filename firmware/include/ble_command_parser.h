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
//   - Two-byte reset command per Spec 02 Section 4a: '0' followed by 'L' or 'R'.
// Returns true if the command was recognized and changed the state, in which
// case the caller should send a state Notify. Returns false for unrecognized
// or no-op commands (including 'U' with no saved previous state per Spec 01
// Section 5a), in which case no Notify is sent.
bool handleBleCommand(const std::string& value, GameState& state);

#endif // BLE_COMMAND_PARSER_H
