// Pickleball scoreboard firmware entry point.
// Wires the game state, BLE service, and display rendering modules together;
// the logic lives in game_state.cpp, ble_service.cpp, display_render_logic.cpp,
// and display_render.cpp. See docs/specs/ for authoritative behavior.

#include <Arduino.h>

#include "game_state.h"
#include "ble_service.h"
#include "display_render.h"

// The Arduino main is excluded during PlatformIO unit tests so that the
// test runner can provide its own setup()/loop().
#ifndef UNIT_TEST

void setup() {
    Serial.begin(115200);

    initDisplay();
    // startBleService() initializes the game state and sends the initial
    // state Notify (Spec 02 Section 6).
    startBleService();
}

void loop() {
    // BLE command processing and state updates happen in the service callback.
    // Re-render the current state every loop (Spec 03 Section 5).
    renderState(getGameState());
    delay(10);
}

#endif // UNIT_TEST
