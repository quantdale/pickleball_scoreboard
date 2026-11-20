#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "game_state.h"

// Spec 02 Sections 3/6: initialize state BEFORE advertising, sync after CCCD.
// brightnessLevel is the level already applied to the display (Spec 03 §2a).
bool startBleService(uint8_t brightnessLevel);
// Only call from the Arduino loop: scoring/undo/rendering have a single owner.
// Returns whether the scoring position changed; brightness is applied in place.
bool processBleCommands();
// Loop only: persists a settled brightness change (Spec 03 §2a).
void saveBrightnessIfDue();
const GameState& getGameState();
uint8_t getBrightnessLevel();
void notifyState(const GameState& state);

#ifdef PICKLE_OTA_ENABLED
// Spec 04 Section 3: no BLE deinit, state reset or discarded acknowledged writes.
bool bleClientConnected();
uint32_t bleActivityCount();
bool beginBleMaintenance();
bool bleMaintenanceExclusive();
void endBleMaintenance();
#endif

#endif
