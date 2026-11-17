#ifdef PICKLE_OTA_ENABLED
// Spec04 §8: authentication callback never starts Wi-Fi or changes game state.
#include "ble_activation.h"
#include "ble_activation_core.h"
#include <Arduino.h>
#include <BLECharacteristic.h>
#include <BLEService.h>
#include <Preferences.h>
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/platform_util.h>
#include "ota_credentials.h"

namespace {
ota::ActivationGate gate;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
struct Lock { Lock() { portENTER_CRITICAL(&mux); } ~Lock() { portEXIT_CRITICAL(&mux); } };
bool authorized = false, ready = false, connected = false, configured = false;
uint32_t authorizedAt = 0, expectedActivity = 0;
uint8_t key[32]{};

class Callbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic* characteristic) override {
        uint8_t challenge[39]; bool ok, available;
        { Lock lock; ok = authorized; available = configured && connected;
            std::memcpy(challenge, gate.bytes(), sizeof(challenge)); }
        if (ok) characteristic->setValue("OK");
        else if (available) characteristic->setValue(challenge, sizeof(challenge));
        else characteristic->setValue("NO");
    }
    void onWrite(BLECharacteristic* characteristic) override {
        const std::string response = characteristic->getValue();
        uint8_t message[52]{}, digest[32]{}, localKey[32]{};
        // Domain12 (including NUL) + challenge39 + explicit authorization flag1.
        std::memcpy(message, "PKS-MAINT-1", 12);
        { Lock lock;
            if (!configured || !connected) return;
            std::memcpy(message+12, gate.bytes(), 39);
            std::memcpy(localKey, key, 32);
        }
        message[51] = response.size() >= 2 ? uint8_t(response[1]) : 0;
        const bool hashed = mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
            localKey, sizeof(localKey), message, sizeof(message), digest) == 0;
        { Lock lock;
            authorized = gate.accept(millis(), reinterpret_cast<const uint8_t*>(response.data()), response.size(), digest) && hashed;
            if (authorized) authorizedAt = millis();
        }
        mbedtls_platform_zeroize(localKey, sizeof(localKey));
        mbedtls_platform_zeroize(digest, sizeof(digest));
    }
};
}

// Called once before advertising; record remains independent of signing private key.
void addBleActivationCharacteristic(BLEService* service)
{
    ota::Credentials record{};
    Preferences storage;
    if (storage.begin("pksmaint", true)) {
        if (storage.getBytesLength("record") == sizeof(record) &&
            storage.getBytes("record", &record, sizeof(record)) == sizeof(record) && ota::credentialShapeValid(record)) {
            std::memcpy(key, record.adminHash, sizeof(key)); configured = true;
        }
        storage.end();
    }
    mbedtls_platform_zeroize(&record, sizeof(record));
    auto characteristic = service->createCharacteristic("beb5483e-36e1-4688-b7f5-ea07361b26aa",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
    characteristic->setCallbacks(new Callbacks());
}
void bleActivationConnected()
{
    uint8_t challenge[39]{}; challenge[0] = 1;
    esp_efuse_mac_get_default(challenge+1); esp_fill_random(challenge+7, 32);
    Lock lock; connected = true; authorized = ready = false;
    if (!configured || !gate.issue(millis(), challenge)) gate.invalidate();
}
void bleActivationDisconnected(uint32_t activity)
{
    Lock lock; connected = false; gate.invalidate();
    ready = authorized && !ota::elapsed(millis(), authorizedAt, 20000);
    expectedActivity = activity;
}
void cancelBleActivation()
{
    Lock lock; authorized = ready = false; gate.invalidate();
}
bool takeBleActivation(uint32_t activity)
{
    Lock lock;
    if (!ready || connected) return false;
    ready = false;
    const bool valid = authorized && activity == expectedActivity && !ota::elapsed(millis(), authorizedAt, 20000);
    authorized = false;
    return valid;
}
#endif
