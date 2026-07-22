// Pickleball scoreboard firmware entry point.
// This change only wires modules together; logic lives in follow-up changes.
// See docs/specs/ for authoritative behavior.

#include <Arduino.h>

#include "game_state.h"
#include "ble_service.h"
#include "display_render.h"

void setup() {
    Serial.begin(115200);

    initDisplay();
    startBleService();

    GameState state;
    initGameState(state, Side::LEFT);

    // Send an initial state notify so a connecting app syncs immediately
    // (Spec 02 Section 6).
    notifyState(state);
}

void loop() {
    // TODO: process incoming BLE commands, update state, and re-render.
    delay(10);
}
