// Spec 02: legacy and manual state streams; initial sync AFTER subscription.
#include "ble_service.h"
#include "ble_advertising_guard.h"
#include "ble_command_ingress.h"
#include "ble_sync_gate.h"
#include "display_settings_store.h"
#include "display_render.h"
#include "state_payload.h"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLECharacteristic.h>
#include <BLE2902.h>
#include <atomic>
#include <esp_gatts_api.h>
#include <esp_heap_caps.h>
#ifdef PICKLE_OTA_ENABLED
#include "ble_activation.h"
#include "ota_memory.h"
#include "ota_maintenance.h"
#include <esp_bt.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {
BLE2902* cccd = nullptr;
BLE2902* manualCccd = nullptr;
GameState currentState; // Spec 01 §6: dedicated RAM, never grows from the heap.
DisplaySettings settings; // Spec 03 §§2a/3: display settings, outside GameState and Undo.
BleSyncGate syncGate, manualSyncGate;
StaticQueue_t commandQueueStorage;
uint8_t commandRecords[SCORING_QUEUE_DEPTH * sizeof(ScoringWrite)];
QueueHandle_t commands = nullptr;
std::atomic<uint16_t> commandHandle{0}, stateHandle{0}, manualHandle{0};
std::atomic<esp_gatt_if_t> gattsInterface{ESP_GATT_IF_NONE};
std::atomic<uint16_t> activeConnId{UINT16_MAX};
uint8_t pendingNotifies = 0;
ExecWriteTracker execWriteTracker;
esp_gatts_cb_t arduinoGatts = nullptr;
AdvertisingGuard advertising;
std::atomic<bool> centralConnected{false};
#ifdef PICKLE_OTA_ENABLED
std::atomic<bool> maintenanceRequested{false}, advertisingStopped{false};
std::atomic<uint32_t> activityCount{0};
uint32_t maintenanceSequence = 0;
// Spec04 Section3: boot once in BLE-only mode; never tear down BLE for maintenance.
struct BleOnlyController {
    bool checked(const char* stage, esp_err_t error) {
        if (error != ESP_OK) Serial.printf("OTA BLE init rejected stage=%s error=%s\n", stage, esp_err_to_name(error));
        return error == ESP_OK;
    }
    bool isIdle() {
        if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) return true;
        Serial.println("OTA BLE init rejected: controller already initialized; no live memory release");
        return false;
    }
    bool releaseClassic() {
        const bool ok = checked("release-classic", esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
        logOtaHeap("after-classic-release");
        return ok;
    }
    bool initializeBle() {
        esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
        config.mode = ESP_BT_MODE_BLE;
        return checked("controller-init-ble", esp_bt_controller_init(&config));
    }
    bool enableBle() {
        const bool ok = checked("controller-enable-ble", esp_bt_controller_enable(ESP_BT_MODE_BLE));
        logOtaHeap("after-ble-controller");
        return ok;
    }
};
#endif

// Spec 02 §6: record every advertising start/stop outcome for the loop's guard.
void gapEvent(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param)
{
    if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT) {
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) advertising.started();
        else advertising.startFailed();
    } else if (event == ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT && param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS) {
        advertising.stopped();
#ifdef PICKLE_OTA_ENABLED
        advertisingStopped = true;
#endif
    }
}

void startAdvertising()
{
    advertising.requested(millis());
    BLEDevice::startAdvertising();
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer*) override
    {
        // A bare connection is NOT a subscribed central (Spec 02 Section 6).
        centralConnected = true;
        advertising.stopped(); // the controller stops connectable advertising on connect
#ifdef PICKLE_OTA_ENABLED
        ++activityCount;
        bleActivationConnected();
#endif
        syncGate.connect();
        manualSyncGate.connect();
        cccd->setNotifications(false);
        manualCccd->setNotifications(false);
        Serial.println("BLE connected; waiting for CCCD");
    }
    void onDisconnect(BLEServer*) override
    {
        centralConnected = false;
#ifdef PICKLE_OTA_ENABLED
        ++activityCount;
        bleActivationDisconnected(activityCount.load());
#endif
        syncGate.disconnect();
        manualSyncGate.disconnect();
        cccd->setNotifications(false);
        manualCccd->setNotifications(false);
#ifdef PICKLE_OTA_ENABLED
        if (!maintenanceRequested.load()) startAdvertising();
#else
        startAdvertising();
#endif
        Serial.println("BLE disconnected; game preserved, advertising resumed");
    }
};

class SubscriptionCallbacks : public BLEDescriptorCallbacks {
    void onWrite(BLEDescriptor* descriptor) override
    {
        // Installed Arduino BLE sets the descriptor value BEFORE this callback.
#ifdef PICKLE_OTA_ENABLED
        cancelBleActivation(); // Spec04 §8: scoring subscription cancels maintenance proof
#endif
        if (descriptor == manualCccd) manualSyncGate.subscription(manualCccd->getNotifications());
        else syncGate.subscription(cccd->getNotifications());
        Serial.println("BLE CCCD updated");
    }
};

// Spec 02 §6: Arduino 2.0.17 replies ESP_GATT_OK before calling onWrite.
// Bypass its dispatcher for this one write-only handle. Discovery, CCCDs,
// Notify and the separate maintenance characteristic still use Arduino.
void dispatchGatts(esp_gatts_cb_event_t event, esp_gatt_if_t iface, esp_ble_gatts_cb_param_t* param)
{
    if (event == ESP_GATTS_REG_EVT) {
        gattsInterface.store(iface);
    } else if (event == ESP_GATTS_CONNECT_EVT) {
        gattsInterface.store(iface);
        activeConnId.store(param->connect.conn_id);
        execWriteTracker.reset();
    } else if (event == ESP_GATTS_DISCONNECT_EVT) {
        uint16_t expected = param->disconnect.conn_id;
        activeConnId.compare_exchange_strong(expected, UINT16_MAX);
        execWriteTracker.reset();
    } else if (event == ESP_GATTS_EXEC_WRITE_EVT && !execWriteTracker.takeArduinoExecForward()) {
        if (esp_ble_gatts_send_response(
                iface, param->exec_write.conn_id, param->exec_write.trans_id, ESP_GATT_OK, nullptr) != ESP_OK)
            Serial.println("BLE exec write ATT response failed");
        return;
    }
    const uint16_t handle = commandHandle.load();
    if (event != ESP_GATTS_WRITE_EVT || !handle || param->write.handle != handle) {
        if (event == ESP_GATTS_WRITE_EVT) execWriteTracker.recordPreparedWrite(false, param->write.is_prep);
        arduinoGatts(event, iface, param);
        return;
    }
#ifdef PICKLE_OTA_ENABLED
    ++activityCount;
    cancelBleActivation(); // always interrupt a racing maintenance request
#endif
    receiveScoringWrite(param->write, [](const ScoringWrite& command) {
        return xQueueSend(commands, &command, 0) == pdTRUE;
    }, [&](ScoringReply status) {
        esp_gatt_status_t gattStatus = ESP_GATT_OK;
        switch (status) {
            case ScoringReply::Success: break;
            case ScoringReply::Unsupported: gattStatus = ESP_GATT_REQ_NOT_SUPPORTED; break;
            case ScoringReply::InvalidOffset: gattStatus = ESP_GATT_INVALID_OFFSET; break;
            case ScoringReply::QueueFull: gattStatus = ESP_GATT_INSUF_RESOURCE; break;
        }
        const auto& write = param->write;
        // If the link vanishes here, do not erase or replay an already queued write.
        if (esp_ble_gatts_send_response(iface, write.conn_id, write.trans_id, gattStatus, nullptr) != ESP_OK)
            Serial.println("BLE scoring ATT response failed");
    });
}
} // namespace

extern "C" esp_err_t __real_esp_ble_gatts_register_callback(esp_gatts_cb_t callback);
extern "C" esp_err_t __wrap_esp_ble_gatts_register_callback(esp_gatts_cb_t callback)
{
    arduinoGatts = callback;
    return __real_esp_ble_gatts_register_callback(dispatchGatts);
}

bool startBleService(const DisplaySettings& restored)
{
    initGameState(currentState); // Spec 01 Section 4, before advertising
    settings = restored;
    commands = xQueueCreateStatic(SCORING_QUEUE_DEPTH, sizeof(ScoringWrite), commandRecords, &commandQueueStorage);
    if (!commands) return false;
#ifdef PICKLE_OTA_ENABLED
    logOtaHeap("before-ble-controller");
    BleOnlyController controller;
    if (!ota::initializeBleOnlyController(controller)) return false;
    // Arduino btStart sees the enabled BLE controller and cannot re-enable BTDM here.
#endif
    BLEDevice::init("PickleScore");
    BLEDevice::setCustomGapHandler(gapEvent);
    BLEServer* server = BLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    BLEService* service = server->createService("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
    BLECharacteristic* command = service->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a8", BLECharacteristic::PROPERTY_WRITE);
    BLECharacteristic* stateCharacteristic = service->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a9", BLECharacteristic::PROPERTY_NOTIFY);
    cccd = new BLE2902();
    cccd->setCallbacks(new SubscriptionCallbacks());
    stateCharacteristic->addDescriptor(cccd);
    // Spec 02 §3: ...26ab (portrait extended state) is retired and never reused.
    BLECharacteristic* manualCharacteristic = service->createCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26ac", BLECharacteristic::PROPERTY_NOTIFY);
    manualCccd = new BLE2902();
    manualCccd->setCallbacks(new SubscriptionCallbacks());
    manualCharacteristic->addDescriptor(manualCccd);
#ifdef PICKLE_OTA_ENABLED
    addBleActivationCharacteristic(service);
#endif
    service->start();
    const uint16_t handle = command->getHandle();
    const uint16_t sHandle = stateCharacteristic->getHandle();
    const uint16_t mHandle = manualCharacteristic->getHandle();
    if (!arduinoGatts || handle == 0 || handle == UINT16_MAX ||
        sHandle == 0 || sHandle == UINT16_MAX || mHandle == 0 || mHandle == UINT16_MAX) return false;
    commandHandle.store(handle);
    stateHandle.store(sHandle);
    manualHandle.store(mHandle);
    BLEAdvertising* advertisement = BLEDevice::getAdvertising();
    advertisement->addServiceUUID(service->getUUID());
    advertisement->setScanResponse(true);
    startAdvertising();
    Serial.println("BLE advertising PickleScore; state owner=Arduino loop");
#ifdef PICKLE_OTA_ENABLED
    logOtaHeap("after-ble-service");
#endif
    return true;
}

static bool sendNotification(uint16_t handle, const std::string& payload)
{
    const esp_gatt_if_t iface = gattsInterface.load();
    const uint16_t connId = activeConnId.load();
    if (!handle || iface == ESP_GATT_IF_NONE || connId == UINT16_MAX) return false;
    return esp_ble_gatts_send_indicate(
        iface, connId, handle, static_cast<uint16_t>(payload.size()),
        reinterpret_cast<uint8_t*>(const_cast<char*>(payload.data())), false) == ESP_OK;
}

static bool sendLegacyState(const GameState& state)
{
    if (!syncGate.canNotify()) return true;
    const std::string payload = serializeStatePayload(state);
    if (!sendNotification(stateHandle.load(), payload)) return false;
    Serial.printf("BLE STATE %s\n", payload.c_str());
    return true;
}

static bool notifyManualState(const GameState& state)
{
    if (!manualSyncGate.canNotify()) return true;
    const std::string payload = serializeManualStatePayload(state);
    if (!sendNotification(manualHandle.load(), payload)) return false;
    Serial.printf("BLE STATE MANUAL %s\n", payload.c_str());
    return true;
}

// Spec 02 §5: the brightness report shares the Manual State stream and its CCCD.
static bool notifyBrightness()
{
    if (!manualSyncGate.canNotify()) return true;
    const std::string payload = serializeBrightnessPayload(settings.level());
    if (!sendNotification(manualHandle.load(), payload)) return false;
    Serial.printf("BLE BRIGHTNESS %s value=%u\n", payload.c_str(), unsigned(brightnessValue(settings.level())));
    return true;
}

// Spec 02 §5a: the color report follows the brightness report on the same stream.
static bool notifyColor()
{
    if (!manualSyncGate.canNotify()) return true;
    const std::string payload = serializeColorPayload(settings.color());
    if (!sendNotification(manualHandle.load(), payload)) return false;
    Serial.printf("BLE COLOR %s\n", payload.c_str());
    return true;
}

static void recordNotify(uint8_t bit, bool sent)
{
    if (sent) pendingNotifies &= uint8_t(~bit);
    else pendingNotifies |= bit;
}

bool processBleCommands()
{
    bool changed = false;
    if (!syncGate.canNotify()) pendingNotifies &= uint8_t(~PENDING_LEGACY_STATE);
    if (!manualSyncGate.canNotify())
        pendingNotifies &= uint8_t(~(PENDING_MANUAL_STATE | PENDING_BRIGHTNESS | PENDING_COLOR));
    ScoringWrite command{};
    // Bound each loop iteration even if a central floods the input.
    for (std::size_t count = 0; count < SCORING_QUEUE_DEPTH && xQueueReceive(commands, &command, 0) == pdTRUE; ++count) {
        const auto effect = applyBoardCommand(command.bytes, command.length, currentState, settings, millis());
        if (effect.brightnessChanged) {
            // Spec 03 §2a: applied before it is reported; a repeated level does not notify.
            setDisplayBrightness(brightnessValue(settings.level()));
            recordNotify(PENDING_BRIGHTNESS, notifyBrightness());
        }
        if (effect.colorChanged) {
            // Spec 03 §3: the loop repaints once after this batch.
            setDisplayColor(ledColor565(settings.color()));
            changed = true;
            recordNotify(PENDING_COLOR, notifyColor());
        }
        if (effect.visibleChange) {
            changed = true;
            notifyState(currentState);
        }
        if (effect.visibleChange || effect.undoStatusChanged)
            recordNotify(PENDING_MANUAL_STATE, notifyManualState(currentState));
    }
    if (syncGate.takeSync()) pendingNotifies |= PENDING_LEGACY_STATE;
    if (manualSyncGate.takeSync())
        pendingNotifies |= PENDING_MANUAL_STATE | PENDING_BRIGHTNESS | PENDING_COLOR;
    if ((pendingNotifies & PENDING_LEGACY_STATE) && syncGate.canNotify()) notifyState(currentState);
    if (manualSyncGate.canNotify()) {
        // Spec 02 §6: state first, it gates readiness.
        if (pendingNotifies & PENDING_MANUAL_STATE)
            recordNotify(PENDING_MANUAL_STATE, notifyManualState(currentState));
        if (!(pendingNotifies & PENDING_MANUAL_STATE)) {
            if (pendingNotifies & PENDING_BRIGHTNESS) recordNotify(PENDING_BRIGHTNESS, notifyBrightness());
            if (pendingNotifies & PENDING_COLOR) recordNotify(PENDING_COLOR, notifyColor());
        }
    }
    return changed;
}

void saveDisplaySettingsIfDue()
{
#ifdef PICKLE_OTA_ENABLED
    if (maintenanceRequested.load()) return; // no NVS work during a maintenance session
#endif
    const uint32_t now = millis();
    if (!settings.saveDue(now)) return;
    const bool ok = storeDisplaySettings(settings);
    settings.saved(ok, now);
    Serial.printf("DISPLAY SETTINGS %s level=%u color=%c\n", ok ? "saved" : "save failed",
                  unsigned(settings.level()), static_cast<char>(settings.color()));
}

const DisplaySettings& getDisplaySettings() { return settings; }

void maintainAdvertising()
{
#ifdef PICKLE_OTA_ENABLED
    const bool maintenance = maintenanceRequested.load();
#else
    const bool maintenance = false;
#endif
    if (!advertising.retryDue(centralConnected.load(), maintenance, millis())) return;
    Serial.println("BLE advertising not confirmed; restarting");
    startAdvertising();
}

#ifdef PICKLE_OTA_ENABLED
bool bleClientConnected() { return centralConnected.load(); }
uint32_t bleActivityCount() { return activityCount.load(); }
bool bleMaintenanceExclusive()
{
    return maintenanceRequested.load() && advertisingStopped.load() && !centralConnected.load() &&
        activityCount.load() == maintenanceSequence && uxQueueMessagesWaiting(commands) == 0;
}
void endBleMaintenance()
{
    const bool wasRequested = maintenanceRequested.exchange(false);
    if (wasRequested && !centralConnected.load()) startAdvertising();
}
bool beginBleMaintenance()
{
    if (centralConnected.load() || uxQueueMessagesWaiting(commands)) return false;
    maintenanceSequence = activityCount.load();
    maintenanceRequested = true;
    advertisingStopped = false;
    BLEDevice::stopAdvertising();
    const uint32_t start = millis();
    while (uint32_t(millis() - start) < 1000) {
        if (centralConnected.load() || activityCount.load() != maintenanceSequence) break;
        if (uint32_t(millis() - start) >= 500 && bleMaintenanceExclusive()) return true;
        delay(5); // loop() still processes any acknowledged command
    }
    endBleMaintenance();
    return false;
}
#endif

const GameState& getGameState() { return currentState; }

void notifyState(const GameState& state)
{
    recordNotify(PENDING_LEGACY_STATE, sendLegacyState(state));
}
