// BLE command parser interface.
// Parses command bytes from the Android app and dispatches to the scoring state.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#ifndef BLE_COMMAND_PARSER_H
#define BLE_COMMAND_PARSER_H

#include <cstddef>
#include <cstdint>
#include <string>

#include "display_brightness.h"
#include "game_state.h"

// Spec 02 §4: exact command values only. One byte: 'S', 'C', 'E', 'U'.
// Two bytes: "+L", "-L", "+R", "-R", "0D", "0S". Anything else, including
// every portrait rally and three-byte reset command, is malformed and ignored.
// Returns whether the six-field projection changed. The caller separately
// checks Undo status for the Manual State stream (Spec 02 §5).
bool isScoringCommand(const char* bytes, std::size_t length);
// Spec 02 §4: scoring commands plus "B1".."B<max>" brightness; only these are queued.
// handleBleCommand ignores brightness: it never touches GameState or Undo.
bool isBoardCommand(const char* bytes, std::size_t length);
bool handleBleCommand(const char* bytes, std::size_t length, GameState& state);
inline bool handleBleCommand(const std::string& value, GameState& state) {
    return handleBleCommand(value.data(), value.size(), state);
}

// Spec 02 §§4–5: one queued command, as the state-owning loop applies it.
// Brightness and scoring never touch each other's state.
struct BoardCommandEffect {
    bool visibleChange;     // Legacy and Manual State
    bool undoStatusChanged; // Manual State only
    bool brightnessChanged; // display and brightness report
};
BoardCommandEffect applyBoardCommand(const char* bytes, std::size_t length, GameState& state,
                                     BrightnessSetting& brightness, uint32_t now);

#endif // BLE_COMMAND_PARSER_H
