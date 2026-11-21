#ifndef BLE_ADVERTISING_GUARD_H
#define BLE_ADVERTISING_GUARD_H

#include <atomic>
#include <cstdint>

// Spec 02 §6: a disconnected board must stay discoverable. Arduino starts
// advertising once per disconnect and never reports a failed start, which would
// leave the board unreachable until a power cycle. GAP callbacks record the
// outcome; the loop restarts advertising that failed or never confirmed.
class AdvertisingGuard {
public:
    static constexpr uint32_t RETRY_MS = 2000;
    void requested(uint32_t now) { requestedAt = now; state = Pending; }
    void started() { state = Active; }
    void startFailed() { state = Failed; }
    // A connection or a confirmed stop ends advertising without a failure.
    void stopped() { state = Idle; }
    bool retryDue(bool connected, bool maintenance, uint32_t now) const
    {
        const uint8_t current = state.load();
        return !connected && !maintenance && (current == Pending || current == Failed) &&
            uint32_t(now - requestedAt.load()) >= RETRY_MS;
    }
    bool active() const { return state.load() == Active; }

private:
    enum : uint8_t { Idle, Pending, Active, Failed };
    std::atomic<uint8_t> state{Idle};
    std::atomic<uint32_t> requestedAt{0};
};

#endif
