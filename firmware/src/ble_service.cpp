// BLE service implementation.
// GATT setup and command/state handling per Spec 02.

#include "ble_service.h"
#include "ble_command_parser.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Spec 02 Section 3 UUIDs.
static const char* DEVICE_NAME = "PickleScore";
static BLEUUID SERVICE_UUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
static BLEUUID COMMAND_CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");
static BLEUUID STATE_CHAR_UUID("beb5483e-36e1-4688-b7f5-ea07361b26a9");

static BLEServer* pServer = nullptr;
static BLECharacteristic* pStateCharacteristic = nullptr;

// Authoritative game state owned by the BLE service.
static GameState currentState;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        // Immediately sync current state to a reconnecting central (Spec 02 Section 6).
        notifyState(currentState);
    }

    void onDisconnect(BLEServer*) override {
        // Resume advertising so the app can reconnect (Spec 02 Section 6).
        BLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        std::string value = characteristic->getValue();
        if (handleBleCommand(value, currentState)) {
            notifyState(currentState);
        }
    }
};

void startBleService() {
    BLEDevice::init(DEVICE_NAME);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Command characteristic: Write-only, App -> ESP32 (Spec 02 Section 3).
    BLECharacteristic* pCommandCharacteristic = pService->createCharacteristic(
        COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    // State characteristic: Notify-only, ESP32 -> App (Spec 02 Section 3).
    pStateCharacteristic = pService->createCharacteristic(
        STATE_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY);
    pStateCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    // Default boot state: left side is 0-0-2. The user can change this with a
    // reset command from the app (Spec 01 Section 4, Spec 02 Section 4a).
    initGameState(currentState, Side::LEFT);
    notifyState(currentState);
}

GameState& getGameState() {
    return currentState;
}

void notifyState(const GameState& state) {
    if (pStateCharacteristic == nullptr) {
        return;
    }

    // Spec 02 Section 5: "<leftScore>,<rightScore>,<servingSide>,<serverNumber>,<gameEnded>".
    char payload[32];
    char sideChar = (state.servingSide == Side::LEFT) ? 'L' : 'R';
    char endedChar = state.gameEnded ? '1' : '0';
    snprintf(payload, sizeof(payload), "%d,%d,%c,%d,%c",
             state.leftScore, state.rightScore, sideChar, state.serverNumber, endedChar);

    pStateCharacteristic->setValue(payload);
    pStateCharacteristic->notify();
}
