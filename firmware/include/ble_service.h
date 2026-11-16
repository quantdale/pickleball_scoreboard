#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "game_state.h"

// Spec 02 Sections 3/6: initialize state BEFORE advertising, sync after CCCD.
bool startBleService();
// Only call from the Arduino loop: scoring/undo/rendering have a single owner.
bool processBleCommands();
const GameState& getGameState();
void notifyState(const GameState& state);

#endif
