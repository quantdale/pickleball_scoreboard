// Production scoreboard: ESP32 owns scoring and redraws only on actual changes.
#include <Arduino.h>
#include "ble_service.h"
#include "display_render.h"
#include "state_payload.h"
#ifdef PICKLE_OTA_ENABLED
#include "ota_maintenance.h"
#endif

#ifndef UNIT_TEST
void setup()
{
#ifdef PICKLE_OTA_ENABLED
    Serial.setRxBufferSize(512);
#endif
    Serial.begin(115200);
    delay(500);
#ifdef PICKLE_OTA_ENABLED
    prepareOtaBootValidation();
    logOtaHeap("before-display");
#endif
    if (!initDisplay()) {
        while (true) delay(1000); // do not operate an uninitialized display
    }
    renderState(getGameState()); // Spec 03 §5: static boot position, before BLE init
#ifdef PICKLE_OTA_ENABLED
    logOtaHeap("after-display");
#endif
    if (!startBleService()) {
        Serial.println("ERROR: BLE initialization failed");
        while (true) delay(1000);
    }
#ifdef PICKLE_OTA_ENABLED
    if (!startOtaMaintenance()) failOtaBootValidation();
#endif
    Serial.printf("SCOREBOARD ready state=%s heap=%u undo_budget=%u\n",
                  serializeStatePayload(getGameState()).c_str(), ESP.getFreeHeap(),
                  unsigned(GameState::HISTORY_BYTES));
}

void loop()
{
    if (processBleCommands()) {
        const auto& state = getGameState();
        renderState(state);
        // Print::printf allocates for long lines in Arduino 2.0.17.
        char line[128];
        const int count = std::snprintf(line, sizeof(line), "DISPLAY state=%s heap=%u min_heap=%u undo_bytes=%u\n",
            serializeStatePayload(state).c_str(), ESP.getFreeHeap(), ESP.getMinFreeHeap(),
            unsigned(state.historyBytesUsed()));
        if (count > 0 && std::size_t(count) < sizeof(line))
            Serial.write(reinterpret_cast<const uint8_t*>(line), std::size_t(count));
    }
#ifdef PICKLE_OTA_ENABLED
    pollOtaMaintenance();
#endif
    delay(10);
}
#endif
