// BLE service implementation stub.
// GATT setup per Spec 02 Section 3; command/state handling logic is TODO.

#include "ble_service.h"

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

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override {
        // TODO: optionally log connection.
    }

    void onDisconnect(BLEServer*) override {
        // Resume advertising so the app can reconnect (Spec 02 Section 6).
        BLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override {
        // TODO: parse command bytes per Spec 02 Section 4 and dispatch.
        (void)characteristic;
    }
};

void startBleService() {
    BLEDevice::init(DEVICE_NAME);

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService* pService = pServer->createService(SERVICE_UUID);

    // Command characteristic: Write-only, App -> ESP32.
    BLECharacteristic* pCommandCharacteristic = pService->createCharacteristic(
        COMMAND_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pCommandCharacteristic->setCallbacks(new CommandCallbacks());

    // State characteristic: Notify-only, ESP32 -> App.
    pStateCharacteristic = pService->createCharacteristic(
        STATE_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY);
    pStateCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();
}

void notifyState(const GameState& state) {
    if (pStateCharacteristic == nullptr) {
        return;
    }

    // TODO: serialize state per Spec 02 Section 5 (<left>,<right>,<side>,<srv>,<ended>).
    pStateCharacteristic->setValue("0,0,L,2,0");
    pStateCharacteristic->notify();

    (void)state;
}
