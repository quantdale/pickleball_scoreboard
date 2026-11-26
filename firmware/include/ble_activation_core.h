#ifndef PICKLESCORE_BLE_ACTIVATION_CORE_H
#define PICKLESCORE_BLE_ACTIVATION_CORE_H
#include "ota_core.h"

// Spec04 §8: pure one-use/replay/rate policy; ESP adapter computes HMAC-SHA256.
namespace ota {
class ActivationGate {
public:
    bool issue(uint32_t now, const uint8_t* fresh) {
        invalidate();
        if (blocked && !elapsed(now, blockedAt, 60000)) return false;
        if (blocked) { blocked = false; failures = 0; }
        std::memcpy(challenge, fresh, sizeof(challenge));
        issuedAt = now; available = true; return true;
    }
    bool accept(uint32_t now, const uint8_t* response, size_t size, const uint8_t* expected) {
        const bool live = available && !elapsed(now, issuedAt, 20000);
        available = false; // consume BEFORE verifying; malformed requests cannot retry
        const bool ok = live && size == 34 && response[0] == 1 && response[1] == 1 &&
            equalSecret(reinterpret_cast<const char*>(response+2), reinterpret_cast<const char*>(expected), 32);
        if (!ok && ++failures >= 5) { blocked = true; blockedAt = now; }
        if (ok) failures = 0;
        return ok;
    }
    void invalidate() { available = false; }
    bool isAvailable() const { return available; }
    const uint8_t* bytes() const { return challenge; }
private:
    uint8_t challenge[39]{};
    uint32_t issuedAt = 0, blockedAt = 0;
    unsigned failures = 0;
    bool available = false, blocked = false;
};
}
#endif
