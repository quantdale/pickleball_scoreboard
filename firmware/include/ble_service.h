#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include "display_settings.h"
#include "game_state.h"

// Spec 02 Sections 3/6: initialize state BEFORE advertising, sync after CCCD.
// settings are those already applied to the display (Spec 03 §§2a/3).
bool startBleService(const DisplaySettings& settings);
// Only call from the Arduino loop: scoring/undo/rendering have a single owner.
// Returns whether the frame must be repainted (scoring position or color);
// brightness is applied in place.
bool processBleCommands();
// Loop only: persists settled display settings (Spec 03 §§2a/3).
void saveDisplaySettingsIfDue();
const GameState& getGameState();
const DisplaySettings& getDisplaySettings();
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
