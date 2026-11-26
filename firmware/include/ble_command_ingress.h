#ifndef BLE_COMMAND_INGRESS_H
#define BLE_COMMAND_INGRESS_H

#include "ble_command_parser.h"
#include <cstring>

struct ScoringWrite { uint8_t length; char bytes[3]; };
constexpr std::size_t SCORING_QUEUE_DEPTH = 16;
enum class ScoringReply { Success, Unsupported, InvalidOffset, QueueFull };

// Spec 02 §§4, 6–7: policy for the command handle only. Reply after (never
// before) queue admission. Malformed command values are ignored as before.
// Brightness shares the FIFO, so it never overtakes an earlier scoring write.
template<class Write, class Enqueue, class Reply>
void receiveScoringWrite(const Write& write, Enqueue enqueue, Reply reply) {
    if (!write.need_rsp) return;
    if (write.is_prep) { reply(ScoringReply::Unsupported); return; }
    if (write.offset != 0) { reply(ScoringReply::InvalidOffset); return; }
    const auto* bytes = reinterpret_cast<const char*>(write.value);
    if (!isBoardCommand(bytes, write.len)) { reply(ScoringReply::Success); return; }
    ScoringWrite command{};
    command.length = static_cast<uint8_t>(write.len);
    std::memcpy(command.bytes, bytes, command.length);
    reply(enqueue(command) ? ScoringReply::Success : ScoringReply::QueueFull);
}

// Track whether any non-scoring characteristic prepared a write via Arduino BLE;
// otherwise an orphan Execute Write Request would receive no ATT response.
struct ExecWriteTracker {
    bool arduinoPrepPending = false;
    void recordPreparedWrite(bool isCommandHandle, bool isPrep) {
        if (!isCommandHandle && isPrep) arduinoPrepPending = true;
    }
    bool takeArduinoExecForward() {
        const bool forward = arduinoPrepPending;
        arduinoPrepPending = false;
        return forward;
    }
    void reset() { arduinoPrepPending = false; }
};

enum NotifyPendingBit : uint8_t {
    PENDING_LEGACY_STATE = 1u << 0,
    PENDING_MANUAL_STATE = 1u << 1,
    PENDING_BRIGHTNESS = 1u << 2,
    PENDING_COLOR = 1u << 3,
};

#endif // BLE_COMMAND_INGRESS_H
