// Production scoreboard: ESP32 owns scoring and redraws only on actual changes.
#include <Arduino.h>
#include "ble_service.h"
#include "display_render.h"
#include "state_payload.h"

#ifndef UNIT_TEST
void setup()
{
    Serial.begin(115200);
    delay(500);
    if (!initDisplay()) {
        while (true) delay(1000); // do not operate an uninitialized display
    }
    GameState boot;
    renderState(boot); // Spec 03 Section 5: visible 0-0-2 before BLE allocation
    if (!startBleService()) {
        Serial.println("ERROR: BLE command queue allocation failed");
        while (true) delay(1000);
    }
    Serial.printf("SCOREBOARD ready state=%s heap=%u\n",
                  serializeStatePayload(getGameState()).c_str(), ESP.getFreeHeap());
}

void loop()
{
    if (processBleCommands()) {
        renderState(getGameState());
        Serial.printf("DISPLAY state=%s heap=%u min_heap=%u\n",
                      serializeStatePayload(getGameState()).c_str(),
                      ESP.getFreeHeap(), ESP.getMinFreeHeap());
    }
    delay(10);
}
#endif
