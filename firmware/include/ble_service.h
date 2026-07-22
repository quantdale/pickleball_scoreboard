// BLE service interface.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "game_state.h"

// Start advertising as "PickleScore" with the command/state GATT service.
void startBleService();

// Send the current state as a Notify payload (Spec 02 Section 5).
void notifyState(const GameState& state);

#endif // BLE_SERVICE_H
