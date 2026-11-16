#ifndef BLE_SYNC_GATE_H
#define BLE_SYNC_GATE_H

#include <atomic>

// Spec 02 Section 6: callbacks request sync; only the state-owning loop notifies.
class BleSyncGate {
public:
    void connect() { subscribed = false; pending = false; connected = true; }
    void disconnect() { connected = false; subscribed = false; pending = false; }
    void subscription(bool enabled) { subscribed = enabled; pending = enabled; }
    bool canNotify() const { return connected.load() && subscribed.load(); }
    bool takeSync() { return pending.exchange(false) && canNotify(); }
private:
    std::atomic<bool> connected{false};
    std::atomic<bool> subscribed{false};
    std::atomic<bool> pending{false};
};

#endif
