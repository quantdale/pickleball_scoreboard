// Pickleball scoreboard firmware entry point.
// This change only wires modules together; logic lives in follow-up changes.
// See docs/specs/ for authoritative behavior.

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
    // Re-render the current state every loop; actual drawing is TODO in
    // display_render.cpp (Spec 03).
    renderState(getGameState());
    delay(10);
}

#endif // UNIT_TEST
