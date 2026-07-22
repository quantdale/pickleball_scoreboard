// BLE service interface.
// See docs/specs/02-ble-protocol.md for the authoritative wire format.

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "game_state.h"

// Start advertising as "PickleScore" with the command/state GATT service.
// Initializes the authoritative game state to a default 0-0-2 on the left
// side and sends an initial state Notify (Spec 02 Section 6).
void startBleService();

// Access the authoritative game state owned by the BLE service.
GameState& getGameState();

// Send the given state as a Notify payload (Spec 02 Section 5).
void notifyState(const GameState& state);

#endif // BLE_SERVICE_H
