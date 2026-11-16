// Spec 02: unchanged UUIDs/commands/payload; initial sync AFTER subscription.
#include "ble_service.h"
#include "ble_command_parser.h"
#include "ble_sync_gate.h"
#include "state_payload.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
BLECharacteristic* stateCharacteristic = nullptr;
BLE2902* cccd = nullptr;
GameState currentState;
BleSyncGate syncGate;
struct Command { uint8_t length; char bytes[3]; };
QueueHandle_t commands = nullptr;

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override
    {
        // A bare connection is NOT a subscribed central (Spec 02 Section 6).
        syncGate.connect();
        cccd->setNotifications(false);
        Serial.println("BLE connected; waiting for CCCD");
    }
    void onDisconnect(BLEServer*) override
    {
        syncGate.disconnect();
        cccd->setNotifications(false);
        BLEDevice::startAdvertising();
        Serial.println("BLE disconnected; game preserved, advertising resumed");
    }
};

class SubscriptionCallbacks : public BLEDescriptorCallbacks {
    void onWrite(BLEDescriptor*) override
    {
        // Installed Arduino BLE sets the descriptor value BEFORE this callback.
        syncGate.subscription(cccd->getNotifications());
        Serial.println("BLE CCCD updated");
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic* characteristic) override
    {
        const std::string value = characteristic->getValue();
        if (value.size() != 1 && value.size() != 3) return;
        Command command{};
        command.length = static_cast<uint8_t>(value.size());
        value.copy(command.bytes, value.size());
        // Only loop() touches game/undo/display. No dynamic command allocation.
        if (xQueueSend(commands, &command, 0) != pdTRUE)
            Serial.println("ERROR: BLE command queue full");
    }
};
} // namespace

bool startBleService()
{
    initGameState(currentState, Side::LEFT); // Spec 01 Section 4, before advertising
    commands = xQueueCreate(16, sizeof(Command));
    if (!commands) return false;
    BLEDevice::init("PickleScore");
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    BLEService* service = server->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    BLECharacteristic* command = service->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a8", BLECharacteristic::PROPERTY_WRITE);
    command->setCallbacks(new CommandCallbacks());
    stateCharacteristic = service->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a9", BLECharacteristic::PROPERTY_NOTIFY);
    cccd = new BLE2902();
    cccd->setCallbacks(new SubscriptionCallbacks());
    stateCharacteristic->addDescriptor(cccd);
    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(service->getUUID());
    advertising->setScanResponse(true);
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising PickleScore; state owner=Arduino loop");
    return true;
}

bool processBleCommands()
{
    bool changed = false;
    Command command{};
    // Bound each loop iteration even if a central floods the input.
    for (int count = 0; count < 16 && xQueueReceive(commands, &command, 0) == pdTRUE; ++count) {
        if (handleBleCommand(std::string(command.bytes, command.length), currentState)) {
            changed = true;
            notifyState(currentState);
        }
    }
    if (syncGate.takeSync()) notifyState(currentState);
    return changed;
}

const GameState& getGameState() { return currentState; }

void notifyState(const GameState& state)
{
    if (!stateCharacteristic || !syncGate.canNotify()) return;
    const std::string payload = serializeStatePayload(state);
    stateCharacteristic->setValue(payload);
    stateCharacteristic->notify();
    Serial.printf("BLE STATE %s\n", payload.c_str());
}
