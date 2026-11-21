#ifndef PICKLESCORE_OTA_CORE_H
#define PICKLESCORE_OTA_CORE_H

#include <cstddef>
#include <cstdint>
#include <cstring>

// Spec 04: bounded policy and signed-container/transaction checks shared by host tests.
namespace ota {
constexpr uint32_t SLOT_SIZE = 0x1f0000;
// Spec04 Section3: pinned Arduino2.0.17 dynamic-TX mode requires a nonzero TX cache.
constexpr int WIFI_TX_CACHE_BUFFERS = 4;
// Spec04 Section3: limit buffers for the single station. Two DMA RX buffers is the SDK
// minimum; four dynamic ones hold the pinned TCP window (5,760 B = 4 x 1,436 B MSS).
constexpr int WIFI_STATIC_RX_BUFFERS = 2;
constexpr int WIFI_DYNAMIC_RX_BUFFERS = 4;
constexpr uint32_t WINDOW_MS = 180000;
constexpr uint32_t SESSION_MS = 600000;
constexpr uint32_t TRANSFER_MS = 120000;
// Spec04 §4: consecutive 2 s socket timeouts an upload tolerates (radio or erase stalls).
constexpr unsigned UPLOAD_IDLE_TIMEOUTS = 3;
// Spec04 §8: after an acknowledged close the server stays up until the client closes the
// connection that carried the 200 (it has read it), or this bound passes. The bound
// covers one lwIP retransmission at the pinned 3 s initial RTO.
constexpr uint32_t CLOSE_FLUSH_MS = 5000;
constexpr char TARGET[] = "PKSCORE-ESP32-V1";
constexpr char IMAGE_MARKER[] = "PKSCORE-OTA-ABI1";

#pragma pack(push, 1)
struct PackageHeader {
    char magic[8];
    uint32_t format;
    uint32_t imageSize;
    char target[16];
    char build[32];
    uint8_t digest[32];
    uint16_t signatureLength;
    uint8_t signature[72];
};
#pragma pack(pop)
static_assert(sizeof(PackageHeader) == 170, "Package format must match offline tool");
constexpr size_t SIGNED_BYTES = offsetof(PackageHeader, signatureLength);

inline bool equalSecret(const char* a, const char* b, size_t length)
{
    uint8_t difference = 0;
    for (size_t i = 0; i < length; ++i) difference |= uint8_t(a[i] ^ b[i]);
    return difference == 0;
}
inline bool elapsed(uint32_t now, uint32_t since, uint32_t limit) { return uint32_t(now - since) >= limit; }
inline bool closeFlushing(bool acknowledged, bool replyDelivered, uint32_t now, uint32_t acknowledgedAt)
{
    return acknowledged && !replyDelivered && !elapsed(now, acknowledgedAt, CLOSE_FLUSH_MS);
}
inline bool safeBuild(const char* value, size_t size)
{
    bool ended = false;
    for (size_t i = 0; i < size; ++i) {
        const char c = value[i];
        if (!c) ended = true;
        else if (ended || !((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
    }
    return ended && value[0];
}
// Exact-request predicates are exercised in host tests AND used by HTTP handlers.
inline bool localRequestAllowed(const char* uri, const char* path, const char* host,
                                const char* origin, bool mutation)
{
    return !std::strcmp(uri, path) && (!std::strcmp(host, "192.168.4.1") || !std::strcmp(host, "192.168.4.1:80")) &&
        (!mutation || !std::strcmp(origin, "http://192.168.4.1"));
}
class SessionAuth {
public:
    void issue(const char* tokenValue, const char* nonceValue) {
        for (size_t i = 0; i < 65; ++i) { token[i] = tokenValue[i]; nonce[i] = nonceValue[i]; }
    }
    bool authorize(const char* bearer, const char* suppliedNonce, bool mutation) {
        if (!token[0] || std::strlen(bearer) != 71 || std::memcmp(bearer, "Bearer ", 7) ||
            !equalSecret(bearer+7, token, 64)) return false;
        if (mutation) {
            if (!nonce[0] || std::strlen(suppliedNonce) != 64 || !equalSecret(suppliedNonce, nonce, 64)) return false;
            wipe(nonce, sizeof(nonce)); // consume before side effects, even on failure
        }
        return true;
    }
    void rotate(const char* value) { std::memcpy(nonce, value, 65); }
    const char* currentNonce() const { return nonce; }
    void clear() { wipe(token, sizeof(token)); wipe(nonce, sizeof(nonce)); }
private:
    static void wipe(char* value, size_t size) {
        volatile char* bytes = value; // portable non-elidable erase for ESP32 and MSVC host tests
        while (size--) *bytes++ = 0;
    }
    char token[65]{}, nonce[65]{};
};
struct Slots { uint32_t running, runningSize, next, nextSize, flashSize; };
inline bool validSlots(const Slots& s)
{
    return s.flashSize == 0x400000 && s.runningSize == SLOT_SIZE && s.nextSize == SLOT_SIZE &&
        ((s.running == 0x10000 && s.next == 0x200000) || (s.running == 0x200000 && s.next == 0x10000));
}
inline const char* headerError(const PackageHeader& h, uint64_t total, const Slots& slots)
{
    if (!validSlots(slots)) return "partition layout mismatch; authorized USB migration required";
    if (std::memcmp(h.magic, "PKSOTA01", 8) || h.format != 1) return "invalid package format";
    if (std::memcmp(h.target, TARGET, sizeof(h.target))) return "incompatible scoreboard target";
    if (!safeBuild(h.build, sizeof(h.build))) return "invalid build identifier";
    if (h.imageSize < 288 || h.imageSize > slots.nextSize) return "image size outside inactive-slot limit";
    if (total != uint64_t(sizeof(h)) + h.imageSize) return "package length mismatch";
    if (h.signatureLength < 8 || h.signatureLength > sizeof(h.signature)) return "invalid signature length";
    for (size_t i = h.signatureLength; i < sizeof(h.signature); ++i)
        if (h.signature[i]) return "noncanonical signature padding";
    return nullptr;
}
inline bool imagePrefixValid(const uint8_t* p)
{
    // esp_image_header_t: magic, segment count, ESP32 chip_id at offsets12/13.
    return p[0] == 0xe9 && p[1] > 0 && p[1] <= 16 && p[12] == 0 && p[13] == 0 && p[23] == 1;
}

enum class Phase { Closed, Window, Session, Receiving, Staged, InstallRequested };
struct TransferStatus { uint32_t received, total; const char* build; };
// Spec04 Section4: projection used by the authenticated /api/status response.
inline TransferStatus transferStatus(Phase phase, uint32_t received, uint32_t total, const char* build)
{
    if (phase == Phase::Staged || phase == Phase::InstallRequested) return {received, total, build};
    if (phase == Phase::Receiving) return {received, total, ""};
    return {0, 0, ""};
}
class Policy {
public:
    void start(uint32_t now) { phase = Phase::Window; bootAt = now; failures = 0; }
    void tick(uint32_t now, bool bleActivity) {
        if (bleActivity || (phase == Phase::Window && elapsed(now, bootAt, WINDOW_MS)) ||
            (hasSession() && elapsed(now, sessionAt, SESSION_MS))) phase = Phase::Closed;
        else if (phase == Phase::Receiving && elapsed(now, transferAt, TRANSFER_MS)) phase = Phase::Session;
    }
    bool login(uint32_t now, bool authenticated, bool quiet) {
        tick(now, false);
        if (phase != Phase::Window) return false;
        if (!authenticated) { if (++failures >= 5) phase = Phase::Closed; return false; }
        if (!quiet) return false;
        sessionAt = now; phase = Phase::Session; return true;
    }
    bool begin(uint32_t now) {
        tick(now, false);
        if (phase != Phase::Session) return false;
        transferAt = now; phase = Phase::Receiving; return true;
    }
    bool transferred(uint32_t now) {
        tick(now, false);
        if (phase != Phase::Receiving) return false;
        phase = Phase::Staged; return true;
    }
    bool install(uint32_t now, bool quiet) {
        tick(now, false);
        if (phase != Phase::Staged || !quiet) return false;
        phase = Phase::InstallRequested; return true;
    }
    void failed() { if (phase == Phase::Receiving || phase == Phase::Staged) phase = Phase::Session; }
    void close() { phase = Phase::Closed; }
    bool hasSession() const { return phase != Phase::Closed && phase != Phase::Window; }
    Phase state() const { return phase; }
private:
    Phase phase = Phase::Closed;
    uint32_t bootAt = 0, sessionAt = 0, transferAt = 0;
    unsigned failures = 0;
};

// Real ESP adapter and fault-injection test adapter exercise the same transaction.
class Sink {
public:
    virtual ~Sink() = default;
    virtual bool begin(const PackageHeader&) = 0;
    virtual bool write(const uint8_t*, size_t) = 0;
    virtual bool finish() = 0; // SDK image verification; does NOT select boot
    virtual bool verifyWritten() = 0; // complete readback SHA256
    virtual bool select() = 0;
    virtual void cancel() = 0;
};
class Transfer {
public:
    explicit Transfer(Sink& adapter) : sink(adapter) {}
    bool begin(const PackageHeader& h, uint64_t total, const Slots& slots, bool signatureValid) {
        if (receiving || staged) { error = "duplicate/concurrent update"; return false; }
        error = headerError(h, total, slots);
        if (error) return false;
        if (!signatureValid) return fail("untrusted firmware signature");
        header = h; received = 0; prefixCount = 0; started = false; receiving = true; return true;
    }
    bool write(const uint8_t* data, size_t size) {
        if (!receiving) return fail("no active transfer");
        if (size > header.imageSize - received) return fail("oversized image");
        received += size;
        if (!started) {
            const size_t needed = sizeof(prefix) - prefixCount;
            const size_t take = size < needed ? size : needed;
            if (take) std::memcpy(prefix + prefixCount, data, take);
            prefixCount += take; data += take; size -= take;
            if (prefixCount < sizeof(prefix)) return true;
            if (!imagePrefixValid(prefix)) return fail("invalid/incompatible ESP32 image header");
            if (!sink.begin(header)) return fail("OTA begin failed");
            started = true;
            if (!sink.write(prefix, sizeof(prefix))) return fail("flash write failed");
        }
        if (size && !sink.write(data, size)) return fail("flash write failed");
        return true;
    }
    bool finish() {
        if (!receiving || !started || received != header.imageSize) return fail("truncated image");
        if (!sink.finish()) return fail("image/hash validation failed");
        if (!sink.verifyWritten()) return fail("flash readback hash mismatch");
        receiving = false; started = false; staged = true; return true;
    }
    bool install(bool authenticated, bool radioQuiet) {
        if (!staged || !authenticated || !radioQuiet) return false;
        if (!sink.select()) return fail("boot selection failed; not rebooting");
        staged = false; return true;
    }
    void cancel() { if (started) sink.cancel(); started = receiving = staged = false; }
    const char* message() const { return error ? error : "ok"; }
    size_t bytes() const { return received; }
    bool ready() const { return staged; }
private:
    bool fail(const char* reason) { error = reason; cancel(); return false; }
    Sink& sink;
    PackageHeader header{};
    uint8_t prefix[24]{};
    size_t received = 0, prefixCount = 0;
    bool receiving = false, started = false, staged = false;
    const char* error = nullptr;
};
} // namespace ota
#endif
