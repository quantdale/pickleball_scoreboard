// BLE command parser implementation.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#include "ble_command_parser.h"

static Side byteToSide(char sideByte) {
    return (sideByte == 'L') ? Side::LEFT : Side::RIGHT;
}

bool handleBleCommand(const std::string& value, GameState& state) {
    if (value.empty()) {
        return false;
    }

    GameState before = state;

    if (value.length() == 1) {
        char cmd = value[0];
        switch (cmd) {
            case 'L': // Spec 02 Section 4: RALLY_WON_LEFT.
                handleRallyWonLeft(state);
                break;
            case 'R': // Spec 02 Section 4: RALLY_WON_RIGHT.
                handleRallyWonRight(state);
                break;
            case 'U': // Spec 02 Section 4: UNDO.
                handleUndo(state);
                break;
            case 'C': // Spec 02 Section 4: SWITCH_COURTS.
                handleSwitchCourts(state);
                break;
            case 'E': // Spec 02 Section 4: END_GAME.
                handleEndGame(state);
                break;
            default:
                // Unrecognized single-byte command; ignore silently (Spec 02 Section 7).
                return false;
        }
    } else if (value.length() == 2 && value[0] == '0') {
        // Spec 02 Section 4a: reset to zero, second byte selects 0-0-2 side.
        char sideByte = value[1];
        if (sideByte == 'L' || sideByte == 'R') {
            handleReset(state, byteToSide(sideByte));
        } else {
            // Malformed reset sequence; ignore entirely (Spec 02 Section 7).
            return false;
        }
    } else {
        // Unrecognized or malformed command; ignore silently (Spec 02 Section 7).
        return false;
    }

    // Notify only when the command produced a real state change.
    return state != before;
}
