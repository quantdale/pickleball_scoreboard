#ifdef PICKLE_OTA_ENABLED
// Spec 04: local-only, signed, staged updates. No eFuse/bootloader/partition writes here.
#include "ota_maintenance.h"
#include "ota_core.h"
#include "ota_memory.h"
#include "ota_diagnostics.h"
#include "ota_credentials.h"
#include "ota_web.h"
#include "ble_service.h"
#include "ble_activation.h"
#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <esp_event.h>
#include <esp_http_server.h>
#include <esp_netif.h>
#include <esp_ota_ops.h>
#include <esp_image_format.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#include <lwip/inet.h>
#include <sdkconfig.h>

#ifndef CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
#error "OTA requires the inspected rollback-enabled SDK; do not silently weaken recovery"
#endif
// Arduino would otherwise confirm before display/BLE/setup self-tests.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
// Spec04 Sections3/8: observation only; never alter the SDK timer deletion result.
std::atomic<uint32_t> timerDeleteOK{0}, timerDeleteArmed{0}, timerDeleteOther{0}, timerDeleteCaller{0};
#ifdef PICKLE_OTA_DIAGNOSTICS
std::atomic<uint32_t> failedTimerAddress{0};
#endif
}
extern "C" esp_err_t __real_esp_timer_delete(esp_timer_handle_t timer);
extern "C" esp_err_t __wrap_esp_timer_delete(esp_timer_handle_t timer)
{
    const esp_err_t result = __real_esp_timer_delete(timer);
    if (result == ESP_OK) timerDeleteOK.fetch_add(1, std::memory_order_relaxed);
    else {
        if (result == ESP_ERR_INVALID_STATE) timerDeleteArmed.fetch_add(1, std::memory_order_relaxed);
        else timerDeleteOther.fetch_add(1, std::memory_order_relaxed);
        timerDeleteCaller.store(uint32_t(reinterpret_cast<uintptr_t>(__builtin_return_address(0))), std::memory_order_relaxed);
#ifdef PICKLE_OTA_DIAGNOSTICS
        failedTimerAddress.store(uint32_t(reinterpret_cast<uintptr_t>(timer)), std::memory_order_relaxed);
#endif
    }
    return result;
}

namespace {
#ifndef PICKLESCORE_BUILD_ID
#if OTA_BUILD_VARIANT == 2
#define PICKLESCORE_BUILD_ID "agent-v1-b"
#else
#define PICKLESCORE_BUILD_ID "agent-v1-a"
#endif
#endif
constexpr char BUILD_MARKER[] = "PKSCORE-BUILD:" PICKLESCORE_BUILD_ID;
const char* buildId() { return BUILD_MARKER + 14; }
constexpr uint32_t MIN_HEAP = ota::OPERATION_FREE_HEAP; // original48KiB reserve, not reduced
ota::Credentials credentials{};
ota::Policy policy;
portMUX_TYPE policyMux = portMUX_INITIALIZER_UNLOCKED;
struct Guard { Guard() { portENTER_CRITICAL(&policyMux); } ~Guard() { portEXIT_CRITICAL(&policyMux); } };
httpd_handle_t server = nullptr;
esp_netif_t* apNetif = nullptr;
bool wifiInitialized = false, wifiStarted = false, ownEventLoop = false, provisioned = false;
bool pendingBoot = false;
std::atomic<bool> startupOK{false}, confirmationDone{false};
std::atomic<bool> apStopped{true}, cleanupHealthy{true};
#ifdef PICKLE_OTA_DIAGNOSTICS
std::atomic<uint32_t> apStopEventStack{0};
#endif
std::atomic<uint32_t> closedAt{0};
esp_event_handler_instance_t wifiEvents = nullptr;
char imageSha256[65]{};
uint32_t bootAt = 0, activityAtBoot = 0, installAt = 0, readyAt = 0, healthyLoops = 0;
char deviceId[13]{};
ota::SessionAuth sessionAuth;
std::atomic<bool> transferBusy{false}, networkStopping{false};
StaticTask_t cleanupControl;
TaskHandle_t cleanupTask = nullptr;
// IDF4.4 Xtensa StackType_t is uint8_t and task depth is BYTES, not upstream FreeRTOS words.
constexpr size_t CLEANUP_STACK_BYTES = 4096;
StackType_t cleanupStack[CLEANUP_STACK_BYTES / sizeof(StackType_t)];
static_assert(sizeof(cleanupStack) == CLEANUP_STACK_BYTES, "Cleanup stack must be sized in bytes");
std::atomic<uint32_t> publishedBytes{0}, publishedTotal{0};
char stagedBuild[32]{};
const esp_partition_t* running = nullptr;
const esp_partition_t* nextPartition = nullptr;
ota::Slots slots{};

ota::HeapSample sampleHeap()
{
    return { ESP.getFreeHeap(), ESP.getMinFreeHeap(),
        uint32_t(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        uint32_t(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        uint32_t(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)) };
}
bool heapAvailable(const char* stage, uint32_t freeFloor, uint32_t blockFloor = 0)
{
    const auto sample = sampleHeap();
    if (ota::heapAdmitted(sample, freeFloor, blockFloor)) return true;
    char line[192];
    const int count = std::snprintf(line, sizeof(line),
        "OTA HEAP rejected stage=%s free=%u min=%u free_internal8=%u largest_internal8=%u floor=%u block=%u\n",
        stage, sample.freeInternal, sample.minimumInternal, sample.freeInternal8, sample.largestInternal8, freeFloor, blockFloor);
    if (count > 0) Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(count) < sizeof(line) ? size_t(count) : sizeof(line)-1);
    return false;
}
bool initRejected(const char* stage, esp_err_t error)
{
    char line[160];
    const int count = std::snprintf(line, sizeof(line), "OTA INIT rejected stage=%s error=%s\n", stage, esp_err_to_name(error));
    if (count > 0) Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(count) < sizeof(line) ? size_t(count) : sizeof(line)-1);
    logOtaHeap(stage);
    return false;
}
bool activeSession() { Guard lock; return policy.hasSession(); }
ota::Phase currentPhase() { Guard lock; return policy.state(); }
bool radioQuiet() { return bleMaintenanceExclusive(); }
bool transferAllowed() { return currentPhase() == ota::Phase::Receiving && radioQuiet(); }
void randomHex(char (&destination)[65])
{
    uint8_t value[32]; esp_fill_random(value, sizeof(value));
    const char* hex = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(value); ++i) { destination[2*i] = hex[value[i] >> 4]; destination[2*i+1] = hex[value[i] & 15]; }
    destination[64] = 0;
    mbedtls_platform_zeroize(value, sizeof(value));
}
bool validPublicKey(const ota::Credentials& record)
{
    mbedtls_ecdsa_context context; mbedtls_ecdsa_init(&context);
    bool ok = mbedtls_ecp_group_load(&context.grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
        mbedtls_ecp_point_read_binary(&context.grp, &context.Q, record.publicKey, sizeof(record.publicKey)) == 0 &&
        mbedtls_ecp_check_pubkey(&context.grp, &context.Q) == 0;
    mbedtls_ecdsa_free(&context); return ok;
}
bool verifySignature(const ota::PackageHeader& header)
{
    uint8_t digest[32];
    if (mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(&header), ota::SIGNED_BYTES, digest, 0)) return false;
    mbedtls_ecdsa_context context; mbedtls_ecdsa_init(&context);
    bool ok = mbedtls_ecp_group_load(&context.grp, MBEDTLS_ECP_DP_SECP256R1) == 0 &&
        mbedtls_ecp_point_read_binary(&context.grp, &context.Q, credentials.publicKey, sizeof(credentials.publicKey)) == 0 &&
        mbedtls_ecdsa_read_signature(&context, digest, sizeof(digest), header.signature, header.signatureLength) == 0;
    mbedtls_ecdsa_free(&context); return ok;
}

class FlashSink : public ota::Sink {
public:
    FlashSink() { mbedtls_sha256_init(&hash); }
    bool begin(const ota::PackageHeader& header) override {
        reason = nullptr; expected = header; markerFound = buildFound = false; markerAt = buildAt = 0;
        std::snprintf(expectedBuild, sizeof(expectedBuild), "PKSCORE-BUILD:%s", header.build);
        if (!nextPartition || nextPartition == running || !ota::validSlots(slots)) { reason = "unsafe inactive partition"; return false; }
        if (!heapAvailable("flash-begin", MIN_HEAP, ota::FLASH_SECTOR_BYTES)) { reason = "insufficient or fragmented free heap"; return false; }
        logOtaHeap("before-flash-begin");
        esp_err_t error = esp_ota_begin(nextPartition, OTA_WITH_SEQUENTIAL_WRITES, &handle);
        if (error != ESP_OK) { handle = 0; reason = esp_err_to_name(error); return false; }
        if (mbedtls_sha256_starts_ret(&hash, 0)) { reason = "SHA256 allocation/start failed"; cancel(); return false; }
        logOtaHeap("after-flash-begin");
        return true;
    }
    bool write(const uint8_t* bytes, size_t size) override {
        if (!transferAllowed()) { reason = "session expired or BLE activity detected"; return false; }
        esp_err_t error = esp_ota_write(handle, bytes, size);
        if (size == 24) ota::diag::budgetPoint("first-erase-write-return");
        if (error != ESP_OK) { reason = esp_err_to_name(error); return false; }
        if (mbedtls_sha256_update_ret(&hash, bytes, size)) { reason = "SHA256 update failed"; return false; }
        for (size_t i = 0; i < size; ++i) {
            scan(bytes[i], ota::IMAGE_MARKER, sizeof(ota::IMAGE_MARKER), markerAt, markerFound);
            scan(bytes[i], expectedBuild, std::strlen(expectedBuild)+1, buildAt, buildFound);
        }
        return true;
    }
    bool finish() override {
        uint8_t actual[32];
        if (mbedtls_sha256_finish_ret(&hash, actual) ||
            !ota::equalSecret(reinterpret_cast<const char*>(actual), reinterpret_cast<const char*>(expected.digest), 32)) {
            reason = "received image SHA256 mismatch"; return false;
        }
        if (!markerFound || !buildFound) { reason = "scoreboard ABI/build marker mismatch"; return false; }
        if (!transferAllowed()) { reason = "session expired or BLE activity detected"; return false; }
        const esp_ota_handle_t finished = handle; handle = 0; // esp_ota_end consumes handle on every result
        ota::diag::budgetPoint("before-ota-end");
        esp_err_t error = esp_ota_end(finished);
        ota::diag::budgetPoint("after-ota-end");
        if (error != ESP_OK) { reason = esp_err_to_name(error); return false; }
        esp_image_metadata_t metadata{};
        const esp_partition_pos_t position = {nextPartition->address, nextPartition->size};
        error = esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &position, &metadata);
        ota::diag::budgetPoint("after-image-extent");
        if (error != ESP_OK || metadata.image_len != expected.imageSize) {
            reason = "invalid image extent/trailing data"; return false;
        }
        return true;
    }
    bool verifyWritten() override {
        uint8_t buffer[1024], actual[32];
        ota::diag::budgetPoint("before-readback");
        if (mbedtls_sha256_starts_ret(&hash, 0)) { reason = "readback SHA256 start failed"; return false; }
        for (size_t offset = 0; offset < expected.imageSize;) {
            if (!transferAllowed()) { reason = "verification interrupted by expiry/BLE activity"; return false; }
            size_t count = expected.imageSize-offset;
            if (count > sizeof(buffer)) count = sizeof(buffer);
            esp_err_t error = esp_partition_read(nextPartition, offset, buffer, count);
            if (error != ESP_OK) { reason = esp_err_to_name(error); return false; }
            if (mbedtls_sha256_update_ret(&hash, buffer, count)) { reason = "flash readback SHA256 failed"; return false; }
            offset += count;
            if (!(offset % 16384)) delay(1);
        }
        return mbedtls_sha256_finish_ret(&hash, actual) == 0 &&
            ota::equalSecret(reinterpret_cast<const char*>(actual), reinterpret_cast<const char*>(expected.digest), 32);
    }
    bool select() override {
        if (!radioQuiet()) { reason = "BLE control is active"; return false; }
        esp_ota_img_states_t state;
        if (esp_ota_get_state_partition(running, &state) != ESP_OK ||
            (state != ESP_OTA_IMG_VALID && state != ESP_OTA_IMG_UNDEFINED)) {
            reason = "running application is not confirmed"; return false;
        }
        esp_err_t error = esp_ota_set_boot_partition(nextPartition);
        if (error != ESP_OK) { reason = esp_err_to_name(error); return false; }
        return true;
    }
    void cancel() override { if (handle) { esp_ota_abort(handle); handle = 0; } }
    const char* reason = nullptr;
private:
    static void scan(uint8_t byte, const char* pattern, size_t length, size_t& at, bool& found) {
        if (found) return;
        if (byte == uint8_t(pattern[at])) ++at;
        else at = byte == uint8_t(pattern[0]) ? 1 : 0;
        if (at == length) found = true;
    }
    esp_ota_handle_t handle = 0;
    ota::PackageHeader expected{};
    mbedtls_sha256_context hash;
    char expectedBuild[48]{};
    size_t markerAt = 0, buildAt = 0;
    bool markerFound = false, buildFound = false;
} sink;
ota::Transfer transfer(sink);

esp_err_t reply(httpd_req_t* req, int code, const char* body, const char* type = "application/json")
{
    const char* status = code == 200 ? "200 OK" : code == 202 ? "202 Accepted" : code == 400 ? "400 Bad Request" :
        code == 401 ? "401 Unauthorized" : code == 403 ? "403 Forbidden" : code == 408 ? "408 Request Timeout" :
        code == 409 ? "409 Conflict" : code == 413 ? "413 Payload Too Large" : code == 422 ? "422 Unprocessable Entity" : "503 Service Unavailable";
    httpd_resp_set_status(req, status); httpd_resp_set_type(req, type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
    httpd_resp_set_hdr(req, "Content-Security-Policy", "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
    httpd_resp_set_hdr(req, "Connection", "close");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}
esp_err_t errorReply(httpd_req_t* req, int code, const char* message)
{
    char body[256]; std::snprintf(body, sizeof(body), "{\"error\":\"%s\"}", message);
    return reply(req, code, body);
}
bool headerEquals(httpd_req_t* req, const char* name, const char* expected)
{
    char value[128];
    return httpd_req_get_hdr_value_str(req, name, value, sizeof(value)) == ESP_OK && !std::strcmp(value, expected);
}
bool localRequest(httpd_req_t* req, const char* path, bool mutation)
{
    char host[64]{}, origin[64]{};
    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) return false;
    if (mutation && httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin)) != ESP_OK) return false;
    return ota::localRequestAllowed(req->uri, path, host, origin, mutation);
}
bool authorize(httpd_req_t* req, bool mutation)
{
    char authorization[80]{}, nonce[65]{};
    if (httpd_req_get_hdr_value_str(req, "Authorization", authorization, sizeof(authorization)) != ESP_OK ||
        std::strlen(authorization) != 71 || std::memcmp(authorization, "Bearer ", 7)) return false;
    if (mutation && (httpd_req_get_hdr_value_str(req, "X-CSRF-Token", nonce, sizeof(nonce)) != ESP_OK || std::strlen(nonce) != 64)) return false;
    Guard lock;
    policy.tick(millis(), bleClientConnected() || bleActivityCount() != activityAtBoot);
    if (!policy.hasSession() || (mutation && policy.state() == ota::Phase::InstallRequested) ||
        !sessionAuth.authorize(authorization, nonce, mutation)) return false;
    if (mutation) {
        char fresh[65]; randomHex(fresh); sessionAuth.rotate(fresh);
        mbedtls_platform_zeroize(fresh, sizeof(fresh));
    }
    return true;
}
const char* phaseName(ota::Phase phase)
{
    switch (phase) {
    case ota::Phase::Window: return "window";
    case ota::Phase::Session: return "session";
    case ota::Phase::Receiving: return "receiving";
    case ota::Phase::Staged: return "staged";
    case ota::Phase::InstallRequested: return "installing";
    default: return "closed";
    }
}
esp_err_t infoHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/api/info", false)) return errorReply(req, 403, "invalid local host/path");
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    const bool known = running && esp_ota_get_state_partition(running, &state) == ESP_OK;
    const bool confirmed = known && state == ESP_OTA_IMG_VALID;
    const char* stateName = !known ? "unknown" : confirmed ? "valid" : state == ESP_OTA_IMG_PENDING_VERIFY ? "pending" :
        state == ESP_OTA_IMG_UNDEFINED ? "undefined" : "other";
    char body[512]; std::snprintf(body, sizeof(body),
        "{\"device\":\"PickleScore-%s\",\"build\":\"%s\",\"slot\":\"%s\",\"limit\":%u,\"heap\":%u,"
        "\"agentProtocol\":1,\"imageSha256\":\"%s\",\"bootState\":\"%s\",\"confirmed\":%s,\"healthy\":%s,\"uptimeMs\":%u}",
        deviceId, buildId(), running ? running->label : "unknown", ota::SLOT_SIZE, ESP.getFreeHeap(),
        imageSha256, stateName, confirmed ? "true" : "false", startupOK.load() ? "true" : "false", millis());
    return reply(req, 200, body);
}
esp_err_t pageHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/", false)) return errorReply(req, 403, "invalid local host/path");
    return reply(req, 200, OTA_HTML, "text/html; charset=utf-8");
}
esp_err_t scriptHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/app.js", false)) return errorReply(req, 403, "invalid local host/path");
    return reply(req, 200, OTA_JS, "application/javascript; charset=utf-8");
}
esp_err_t statusHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/api/status", false) || !authorize(req, false)) return errorReply(req, 401, "session unavailable");
    char body[320], nonce[65]; ota::Phase phase;
    { Guard lock; phase = policy.state(); std::memcpy(nonce, sessionAuth.currentNonce(), sizeof(nonce)); }
    const auto status = ota::transferStatus(phase, publishedBytes.load(), publishedTotal.load(), stagedBuild);
    // Spec 04 §8: authenticated resource evidence without opening the enclosure.
    // Historical minimum/HTTP task watermark are observations, not admission floors.
    const auto heap = sampleHeap();
    std::snprintf(body, sizeof(body),
        "{\"phase\":\"%s\",\"nonce\":\"%s\",\"received\":%u,\"total\":%u,\"stagedBuild\":\"%s\","
        "\"heap\":%u,\"minimumHeap\":%u,\"largestInternal8\":%u,\"httpStackFree\":%u}",
        phaseName(phase), nonce, status.received, status.total, status.build, heap.freeInternal,
        heap.minimumInternal, heap.largestInternal8, unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    return reply(req, 200, body);
}
esp_err_t loginHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/api/session", true) || req->content_len) return errorReply(req, 403, "invalid login origin/path/body");
    if (currentPhase() != ota::Phase::Window) return errorReply(req, 409, "maintenance unavailable or already claimed");
    char secret[65]{}; uint8_t digest[32]{};
    bool valid = httpd_req_get_hdr_value_str(req, "X-Admin-Key", secret, sizeof(secret)) == ESP_OK && std::strlen(secret) == 64 &&
        mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(secret), 64, digest, 0) == 0 &&
        ota::equalSecret(reinterpret_cast<const char*>(digest), reinterpret_cast<const char*>(credentials.adminHash), 32);
    mbedtls_platform_zeroize(secret, sizeof(secret)); mbedtls_platform_zeroize(digest, sizeof(digest));
    if (!valid) {
        { Guard lock; policy.login(millis(), false, false); }
        return errorReply(req, 401, "invalid maintenance key");
    }
    if (!beginBleMaintenance()) return errorReply(req, 409, "BLE controller/commands active; maintenance refused");
    char body[192]; bool admitted;
    {
        Guard lock;
        admitted = policy.login(millis(), true, radioQuiet());
        if (admitted) {
            char token[65], nonce[65]; randomHex(token); randomHex(nonce);
            sessionAuth.issue(token, nonce);
            std::snprintf(body, sizeof(body), "{\"token\":\"%s\",\"nonce\":\"%s\"}", token, nonce);
            mbedtls_platform_zeroize(token, sizeof(token)); mbedtls_platform_zeroize(nonce, sizeof(nonce));
        }
    }
    if (!admitted) { endBleMaintenance(); return errorReply(req, 409, "window expired or BLE activity detected"); }
    logOtaHeap("session-admitted");
    return reply(req, 200, body);
}
bool receiveExact(httpd_req_t* req, uint8_t* bytes, size_t size, uint32_t began)
{
    while (size) {
        if (!transferAllowed() || ota::elapsed(millis(), began, ota::TRANSFER_MS)) return false;
        int count = httpd_req_recv(req, reinterpret_cast<char*>(bytes), size);
        if (count <= 0) return false; // including idle socket timeout: no unbounded retry
        bytes += count; size -= size_t(count);
    }
    return true;
}
esp_err_t uploadHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/api/upload", true) || !authorize(req, true)) return errorReply(req, 403, "authorization/origin/nonce rejected");
    if (!headerEquals(req, "Content-Type", "application/octet-stream")) return errorReply(req, 400, "raw signed package required");
    if (req->content_len < sizeof(ota::PackageHeader)+288 || req->content_len > sizeof(ota::PackageHeader)+ota::SLOT_SIZE)
        return errorReply(req, 413, "package outside size limit");
    bool admitted;
    { Guard lock; admitted = radioQuiet() && policy.begin(millis()); }
    if (!admitted) return errorReply(req, 409, "update already active/staged or BLE busy");
    transferBusy = true;
    struct Done { ~Done() { transferBusy = false; } } done;
    publishedBytes = 0; publishedTotal = 0; stagedBuild[0] = 0; sink.reason = nullptr;
    ota::PackageHeader header{};
    const uint32_t began = millis();
    const char* problem = nullptr;
    int status = 422;
    if (!receiveExact(req, reinterpret_cast<uint8_t*>(&header), sizeof(header), began)) { problem = "header truncated, expired or interrupted"; status = 408; }
    if (!problem) problem = ota::headerError(header, req->content_len, slots);
    if (!problem && !heapAvailable("upload-admission", MIN_HEAP, ota::FLASH_SECTOR_BYTES)) { problem = "insufficient or fragmented free heap"; status = 503; }
    if (!problem) {
        logOtaHeap("before-signature");
        bool trusted;
        {
            const ota::diag::Budget budget("signature");
            trusted = verifySignature(header);
        }
        logOtaHeap("after-signature"); // retain low-water evidence even if verification/allocation fails
        if (!trusted) problem = "untrusted firmware signature";
    }
    const ota::diag::Budget uploadBudget("upload-body");
    if (!problem && !transfer.begin(header, req->content_len, slots, true)) problem = transfer.message();
    if (!problem) publishedTotal = header.imageSize;
    uint8_t buffer[1024];
    while (!problem && publishedBytes.load() < header.imageSize) {
        size_t count = header.imageSize-publishedBytes.load();
        if (count > sizeof(buffer)) count = sizeof(buffer);
        if (!receiveExact(req, buffer, count, began)) { problem = "transfer truncated, expired or interrupted"; status = 408; break; }
        if (!transfer.write(buffer, count)) { problem = sink.reason ? sink.reason : transfer.message(); break; }
        publishedBytes = transfer.bytes();
        if (publishedBytes.load() == 16384) ota::diag::budgetPoint("after-16k-written");
        delay(1);
    }
    if (!problem && !transfer.finish()) problem = sink.reason ? sink.reason : transfer.message();
    if (!problem) {
        ota::diag::budgetPoint("after-readback");
        logOtaHeap("after-flash-readback");
        // Spec04 §9: same HTTP task as upload, signature, SDK verification and readback.
        Serial.printf("OTA HTTP stack free=%u bytes\n", unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    }
    if (!problem) {
        Guard lock;
        if (!radioQuiet() || !policy.transferred(millis())) problem = "session expired or BLE activity detected";
        else std::memcpy(stagedBuild, header.build, sizeof(stagedBuild));
    }
    if (problem) {
        transfer.cancel(); { Guard lock; policy.failed(); }
        Serial.printf("OTA rejected: %s\n", problem);
        return errorReply(req, status, problem);
    }
    Serial.printf("OTA staged build=%s bytes=%u heap=%u\n", stagedBuild, publishedBytes.load(), ESP.getFreeHeap());
    return reply(req, 200, "{\"staged\":true,\"message\":\"Verified; explicit install required\"}");
}
esp_err_t installHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/api/install", true) || req->content_len || !authorize(req, true))
        return errorReply(req, 403, "authorization/origin/nonce/body rejected");
    bool admitted;
    { Guard lock;
        admitted = policy.state() == ota::Phase::Staged && !transferBusy.load() && transfer.ready() &&
            policy.install(millis(), radioQuiet());
        if (admitted) installAt = millis();
    }
    if (!admitted) return errorReply(req, 409, "no verified image or BLE control active");
    return reply(req, 202, "{\"installing\":true,\"message\":\"Reboot resets the in-memory score\"}");
}
esp_err_t closeHandler(httpd_req_t* req)
{
    if (!localRequest(req, "/api/close", true) || req->content_len || !authorize(req, true)) return errorReply(req, 403, "authorization/origin/nonce/body rejected");
    { Guard lock; policy.close(); }
    return reply(req, 200, "{\"closed\":true}");
}

void stopNetworking()
{
    logOtaHeap("network-stop-begin");
    if (server) { httpd_stop(server); server = nullptr; } // joins handler before transfer cleanup
    logOtaHeap("after-http-stop");
    transfer.cancel();
    if (wifiInitialized) {
#ifdef PICKLE_OTA_DIAGNOSTICS
        apStopEventStack = 0;
#endif
        apStopped = false;
        const esp_err_t stopped = wifiStarted ? esp_wifi_stop() : ESP_OK;
        const uint32_t began = millis();
        while (wifiStarted && stopped == ESP_OK && !apStopped.load() && !ota::elapsed(millis(), began, 2000)) delay(10);
        const bool stoppedCleanly = stopped == ESP_OK && (!wifiStarted || apStopped.load());
        const esp_err_t deinitialized = stoppedCleanly ? esp_wifi_deinit() : ESP_ERR_INVALID_STATE;
        if (deinitialized != ESP_OK) cleanupHealthy = false;
        Serial.printf("OTA WiFi cleanup stop=%s event=%u deinit=%s\n", esp_err_to_name(stopped), apStopped.load(), esp_err_to_name(deinitialized));
#ifdef PICKLE_OTA_DIAGNOSTICS
        if (apStopped.load()) Serial.printf("OTA AP stop event task stack free=%u bytes\n", apStopEventStack.load());
#endif
        if (deinitialized == ESP_OK) wifiInitialized = wifiStarted = false;
        logOtaHeap("after-wifi-deinit");
    }
    // Spec04 §3: on driver teardown failure leave its dependencies alive; no more windows this boot.
    if (!wifiInitialized) {
        if (wifiEvents) {
            const esp_err_t result = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvents);
            if (result == ESP_OK) wifiEvents = nullptr;
            else cleanupHealthy = false;
#ifdef PICKLE_OTA_OWNERSHIP
            Serial.printf("OTA RESOURCE unregister=%s\n", esp_err_to_name(result));
            logOtaHeap("after-app-handler-unregister");
#endif
        }
        if (apNetif) { esp_netif_destroy_default_wifi(apNetif); apNetif = nullptr; }
#ifdef PICKLE_OTA_OWNERSHIP
        logOtaHeap("after-ap-netif-destroy");
#endif
        if (ownEventLoop) {
            const esp_err_t result = esp_event_loop_delete_default();
            if (result == ESP_OK) { ownEventLoop = false; wifiEvents = nullptr; }
            else cleanupHealthy = false;
#ifdef PICKLE_OTA_OWNERSHIP
            Serial.printf("OTA RESOURCE event_loop_delete=%s\n", esp_err_to_name(result));
            logOtaHeap("after-event-loop-delete");
#endif
        }
    }
    if (!cleanupHealthy.load()) Serial.println("OTA cleanup incomplete; reactivation disabled until reboot");
    { Guard lock; policy.close(); sessionAuth.clear(); }
    endBleMaintenance();
    mbedtls_platform_zeroize(credentials.apPassword, sizeof(credentials.apPassword));
    Serial.printf("OTA maintenance closed; BLE available; heap=%u min_heap=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    logOtaHeap("network-closed");
}
void wifiEvent(void*, esp_event_base_t, int32_t event, void*)
{
    if (event == WIFI_EVENT_AP_STOP) {
#ifdef PICKLE_OTA_DIAGNOSTICS
        apStopEventStack = uxTaskGetStackHighWaterMark(nullptr);
#endif
        apStopped = true;
    }
    if (event == WIFI_EVENT_AP_STACONNECTED) Serial.println("OTA WiFi station associated");
    if (event == WIFI_EVENT_AP_STADISCONNECTED) Serial.println("OTA WiFi station disconnected");
}
bool startNetworking()
{
    logOtaHeap("network-start-begin");
    if (!cleanupHealthy.load()) return initRejected("previous-cleanup", ESP_ERR_INVALID_STATE);
    if (!heapAvailable("network-preinit", ota::STARTUP_FREE_HEAP, ota::HTTP_STACK_BYTES)) return false;
    esp_err_t error = esp_netif_init();
    if (error != ESP_OK) return initRejected("netif-init", error);
    esp_err_t event = esp_event_loop_create_default();
    if (event != ESP_OK && event != ESP_ERR_INVALID_STATE) return initRejected("event-loop", event);
    ownEventLoop = event == ESP_OK;
    apNetif = esp_netif_create_default_wifi_ap();
    if (!apNetif) return initRejected("ap-netif", ESP_ERR_NO_MEM);
    // IDF 4.4.7 dispatches in registration order: let default AP_STOP finish netif stop
    // before publishing AP_STOP to the cleanup worker (Spec04 §3).
    error = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifiEvent, nullptr, &wifiEvents);
    if (error != ESP_OK) return initRejected("wifi-events", error);
    logOtaHeap("after-ap-netif");
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    config.static_rx_buf_num = 4; config.dynamic_rx_buf_num = 8;
    config.tx_buf_type = 1; config.dynamic_tx_buf_num = 8; config.cache_tx_buf_num = ota::WIFI_TX_CACHE_BUFFERS;
    config.ampdu_rx_enable = 0; config.ampdu_tx_enable = 0; config.nvs_enable = 0;
    error = esp_wifi_init(&config);
    if (error != ESP_OK) return initRejected("wifi-init", error);
    wifiInitialized = true;
    logOtaHeap("after-wifi-init");
    wifi_config_t wifi{};
    std::memcpy(wifi.ap.ssid, credentials.ssid, std::strlen(credentials.ssid));
    std::memcpy(wifi.ap.password, credentials.apPassword, 32);
    wifi.ap.ssid_len = std::strlen(credentials.ssid); wifi.ap.channel = 1;
    wifi.ap.authmode = WIFI_AUTH_WPA2_PSK; wifi.ap.max_connection = 1;
    wifi.ap.pairwise_cipher = WIFI_CIPHER_TYPE_CCMP; // explicit AES/CCMP, as Arduino WiFiAP uses
    // IDF4.4 ESP32 AP config has no PMF field; retain supported WPA2-PSK.
    esp_netif_ip_info_t ip{};
    ip.ip.addr = inet_addr("192.168.4.1"); ip.gw.addr = ip.ip.addr; ip.netmask.addr = inet_addr("255.255.255.0");
    esp_netif_dhcps_stop(apNetif);
    error = esp_netif_set_ip_info(apNetif, &ip);
    if (error != ESP_OK) return initRejected("ap-ip", error);
    error = esp_netif_dhcps_start(apNetif);
    if (error != ESP_OK) return initRejected("dhcp-start", error);
    error = esp_wifi_set_mode(WIFI_MODE_AP);
    if (error != ESP_OK) return initRejected("wifi-mode", error);
    error = esp_wifi_set_config(WIFI_IF_AP, &wifi);
    if (error != ESP_OK) return initRejected("wifi-config", error);
    error = esp_wifi_start();
    if (error != ESP_OK) return initRejected("wifi-start", error);
    wifiStarted = true;
    logOtaHeap("after-wifi-start");
    // Pay the known task stack while retaining the original operation reserve; final check includes other HTTP costs.
    if (!heapAvailable("http-stack", ota::HTTP_START_FREE_HEAP, ota::HTTP_STACK_BYTES)) return false;
    httpd_config_t http = HTTPD_DEFAULT_CONFIG();
    http.stack_size = ota::HTTP_STACK_BYTES; http.max_open_sockets = 2; http.max_uri_handlers = 8;
    http.recv_wait_timeout = 2; http.send_wait_timeout = 2; http.lru_purge_enable = true;
    error = httpd_start(&server, &http);
    if (error != ESP_OK) return initRejected("http-start", error);
    logOtaHeap("after-http-start");
    const httpd_uri_t routes[] = {
        {"/", HTTP_GET, pageHandler, nullptr}, {"/app.js", HTTP_GET, scriptHandler, nullptr},
        {"/api/info", HTTP_GET, infoHandler, nullptr}, {"/api/status", HTTP_GET, statusHandler, nullptr},
        {"/api/session", HTTP_POST, loginHandler, nullptr}, {"/api/upload", HTTP_POST, uploadHandler, nullptr},
        {"/api/install", HTTP_POST, installHandler, nullptr}, {"/api/close", HTTP_POST, closeHandler, nullptr}
    };
    for (const auto& route : routes) {
        error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return initRejected("http-route", error);
    }
    logOtaHeap("network-ready");
    return heapAvailable("network-ready", MIN_HEAP, ota::FLASH_SECTOR_BYTES);
}
void requestNetworkStop()
{
    if (networkStopping.exchange(true)) return;
    { Guard lock; policy.close(); }
    // Spec04 §3: httpd_stop can block. Reuse one parked worker, not the stack/TCB of a
    // self-deleted task that the IDF 4.4.7 idle task may not yet have reaped.
    if (!cleanupTask) {
        cleanupTask = xTaskCreateStatic([](void*) {
            for (;;) {
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                stopNetworking();
                const UBaseType_t remaining = uxTaskGetStackHighWaterMark(nullptr);
                Serial.printf("OTA cleanup stack free=%u bytes\n", unsigned(remaining));
                if (remaining < 512) cleanupHealthy = false;
                closedAt = millis();
                networkStopping = false;
            }
        }, "ota-close", sizeof(cleanupStack), nullptr, 2, cleanupStack, &cleanupControl);
        if (!cleanupTask) {
            cleanupHealthy = false;
            Serial.println("OTA cleanup worker unavailable; reactivation disabled until reboot");
            return;
        }
    }
    xTaskNotifyGive(cleanupTask);
}
int unhex(char c) { return c >= '0' && c <= '9' ? c-'0' : c >= 'a' && c <= 'f' ? c-'a'+10 : -1; }
void pollProvisioning()
{
    static char line[360]{}; static size_t used = 0; static bool overflow = false;
    for (int i = 0; i < 64 && Serial.available(); ++i) {
        const char c = char(Serial.read());
        if (c == '\r') continue;
        if (c != '\n') {
            if (used+1 < sizeof(line)) line[used++] = c;
            else overflow = true;
            continue;
        }
        line[used] = 0;
        if (!overflow && used == 8 + sizeof(ota::Credentials)*2 && !std::memcmp(line, "PSPROV1 ", 8) &&
            !ota::elapsed(millis(), bootAt, ota::WINDOW_MS) && !bleClientConnected() &&
            bleActivityCount() == activityAtBoot && !activeSession() && !transferBusy.load() && !networkStopping.load()) {
            ota::Credentials candidate{}; bool ok = true;
            auto bytes = reinterpret_cast<uint8_t*>(&candidate);
            for (size_t at = 0; at < sizeof(candidate); ++at) {
                int a = unhex(line[8+at*2]), b = unhex(line[9+at*2]);
                if (a < 0 || b < 0) { ok = false; break; }
                bytes[at] = uint8_t((a << 4) | b);
            }
            ok = ok && ota::credentialShapeValid(candidate) && validPublicKey(candidate);
            if (ok) {
                if (server || wifiInitialized || apNetif) stopNetworking();
                Preferences storage;
                ok = cleanupHealthy.load() && storage.begin("pksmaint", false);
                if (ok) { ok = storage.putBytes("record", &candidate, sizeof(candidate)) == sizeof(candidate); storage.end(); }
                Serial.println(ok ? "OTA PROVISIONED; reboot manually to open maintenance AP" : "OTA provisioning storage failed");
            } else Serial.println("OTA provisioning rejected");
            mbedtls_platform_zeroize(&candidate, sizeof(candidate));
        } else Serial.println("OTA provisioning refused: format/window/game/session restriction");
        mbedtls_platform_zeroize(line, sizeof(line)); used = 0; overflow = false;
    }
}
} // namespace

void logOtaHeap(const char* stage)
{
    const auto sample = sampleHeap();
    multi_heap_info_t bytes{};
    heap_caps_get_info(&bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    // Public allocator/task counts distinguish live allocations from free-block fragmentation.
    // Timer counts cover linked delete calls, not the number of globally live SDK timers.
    // Do not use Print::printf: long lines allocate a temporary heap buffer in Arduino2.0.17.
    char line[512];
    const int count = std::snprintf(line, sizeof(line),
        "OTA HEAP stage=%s free=%u min=%u free_internal8=%u largest8=%u largest_internal8=%u"
        " tasks=%u allocated8=%u blocks8=%u free_blocks8=%u timer_del_ok=%u timer_del_armed=%u timer_del_other=%u timer_del_caller=%08x"
#ifdef PICKLE_OTA_DIAGNOSTICS
        " timer_del_ptr=%08x"
#endif
        "\n",
        stage, sample.freeInternal, sample.minimumInternal, sample.freeInternal8, sample.largest8, sample.largestInternal8,
        unsigned(uxTaskGetNumberOfTasks()), unsigned(bytes.total_allocated_bytes), unsigned(bytes.allocated_blocks), unsigned(bytes.free_blocks),
        timerDeleteOK.load(std::memory_order_relaxed), timerDeleteArmed.load(std::memory_order_relaxed),
        timerDeleteOther.load(std::memory_order_relaxed), timerDeleteCaller.load(std::memory_order_relaxed)
#ifdef PICKLE_OTA_DIAGNOSTICS
        , failedTimerAddress.load(std::memory_order_relaxed)
#endif
    );
    if (count > 0) Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(count) < sizeof(line) ? size_t(count) : sizeof(line)-1);
}

void prepareOtaBootValidation()
{
    const ota::diag::Budget budget("boot-image-validation");
    bootAt = millis();
    const uint64_t mac = ESP.getEfuseMac();
    std::snprintf(deviceId, sizeof(deviceId), "%012llx", static_cast<unsigned long long>(mac & 0xffffffffffffULL));
    running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    pendingBoot = running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY;
    if (pendingBoot) {
        esp_err_t error = esp_task_wdt_init(30, true);
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) { failOtaBootValidation(); return; }
        if (esp_task_wdt_add(nullptr) != ESP_OK) { failOtaBootValidation(); return; }
    }
    // Exact whole-image digest (including appended image checksum/hash), not app descriptor hash.
    esp_image_metadata_t metadata{};
    const esp_partition_pos_t position = {running ? running->address : 0, running ? running->size : 0};
    if (running && esp_image_verify(ESP_IMAGE_VERIFY_SILENT, &position, &metadata) == ESP_OK) {
        mbedtls_sha256_context hash; mbedtls_sha256_init(&hash);
        uint8_t buffer[1024], digest[32]; bool ok = mbedtls_sha256_starts_ret(&hash, 0) == 0;
        for (size_t offset = 0; ok && offset < metadata.image_len;) {
            const size_t count = metadata.image_len-offset < sizeof(buffer) ? metadata.image_len-offset : sizeof(buffer);
            ok = esp_partition_read(running, offset, buffer, count) == ESP_OK && mbedtls_sha256_update_ret(&hash, buffer, count) == 0;
            offset += count;
        }
        ok = ok && mbedtls_sha256_finish_ret(&hash, digest) == 0;
        if (ok) for (size_t i = 0; i < 32; ++i) std::snprintf(imageSha256+2*i, 3, "%02x", digest[i]);
        mbedtls_sha256_free(&hash);
    }
    Serial.printf("OTA build=%s ABI=%s slot=%s pending=%u\n", buildId(), ota::IMAGE_MARKER, running ? running->label : "unknown", pendingBoot);
#ifdef PICKLE_OTA_ROLLBACK_TEST
    // Spec04 §§7/9: bench fixture only. Reboot without confirming, so the bootloader
    // must retire PENDING_VERIFY as ABORTED. Never deliberately reset a nontrial boot.
    if (pendingBoot && esp_ota_check_rollback_is_possible()) {
        Serial.println("OTA BENCH rollback fixture: unconfirmed trial; restarting");
        Serial.flush();
        ESP.restart();
    }
    Serial.println("OTA BENCH rollback fixture: unexpected state or no eligible fallback; not confirming");
    failOtaBootValidation();
    while (true) delay(1000);
#endif
}
void failOtaBootValidation()
{
    startupOK = false;
    Serial.println("OTA startup self-check failed; maintenance disabled");
    if (pendingBoot) {
        Serial.println("OTA pending application failed; requesting previous valid image");
        esp_err_t error = esp_ota_mark_app_invalid_rollback_and_reboot();
        Serial.printf("OTA rollback returned %s; waiting for pending-boot watchdog\n", esp_err_to_name(error));
        while (true) delay(1000);
    }
}
bool startOtaMaintenance()
{
    ota::diag::startOwnership();
    const ota::diag::Budget budget("maintenance-startup");
    activityAtBoot = bleActivityCount();
    nextPartition = esp_ota_get_next_update_partition(nullptr);
    slots = { running ? running->address : 0, running ? running->size : 0,
        nextPartition ? nextPartition->address : 0, nextPartition ? nextPartition->size : 0, ESP.getFlashChipSize() };
    if (!imageSha256[0] || !ota::validSlots(slots) || !running || !nextPartition || running->type != ESP_PARTITION_TYPE_APP ||
        nextPartition->type != ESP_PARTITION_TYPE_APP ||
        !esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, nullptr)) {
        Serial.println("OTA disabled: partition mismatch; explicit authorized USB migration required"); return false;
    }
    Preferences storage;
    if (storage.begin("pksmaint", true)) {
        if (storage.getBytesLength("record") == sizeof(credentials))
            provisioned = storage.getBytes("record", &credentials, sizeof(credentials)) == sizeof(credentials);
        storage.end();
    }
    provisioned = provisioned && ota::credentialShapeValid(credentials) && validPublicKey(credentials);
    if (!provisioned) {
        Serial.println("OTA disabled: no valid private provisioning; ordinary BLE remains available");
        startupOK = !pendingBoot; return startupOK;
    }
    { Guard lock; policy.start(millis()); }
    if (!startNetworking()) { stopNetworking(); return false; }
    startupOK = true; readyAt = millis();
    Serial.printf("OTA AP ready ssid=%s address=192.168.4.1 window=180s heap=%u min_heap=%u\n", credentials.ssid, ESP.getFreeHeap(), ESP.getMinFreeHeap());
    return true;
}
void pollOtaMaintenance()
{
    pollProvisioning();
    // Spec04 §8: only the loop starts AP, after authenticated disconnect, no pending scoring or cleanup.
    if (!networkStopping.load() && !server && !bleClientConnected() && cleanupHealthy.load() &&
        ota::elapsed(millis(), closedAt.load(), 1000) && takeBleActivation(bleActivityCount())) {
        if (startOtaMaintenance()) Serial.println("OTA authenticated BLE activation ready");
        else Serial.println("OTA authenticated BLE activation failed; ordinary BLE retained");
    }
    if (!networkStopping.load() && server) {
        { Guard lock; policy.tick(millis(), bleClientConnected() || bleActivityCount() != activityAtBoot); }
        if (pendingBoot && !confirmationDone && ESP.getFreeHeap() < ota::EMERGENCY_FREE_HEAP) failOtaBootValidation();
        if (currentPhase() == ota::Phase::Closed || ESP.getFreeHeap() < ota::EMERGENCY_FREE_HEAP) requestNetworkStop();
        else if (currentPhase() == ota::Phase::InstallRequested && ota::elapsed(millis(), installAt, 2000) && !transferBusy.load()) {
            const uint32_t activity = bleActivityCount();
            bool selected = false;
            if (radioQuiet()) {
                const ota::diag::Budget budget("boot-selection");
                selected = transfer.install(true, true);
            }
            if (selected) {
                // Last defensive check; confirmed advertising stop prevents new admissions.
                if (!bleClientConnected() && bleActivityCount() == activity) {
                    Serial.printf("OTA installing build=%s; reboot resets in-memory score\n", stagedBuild);
                    ESP.restart();
                }
                esp_ota_set_boot_partition(running); // activity wins: do NOT reboot a controlled game
            }
            Serial.printf("OTA install cancelled: %s\n", sink.reason ? sink.reason : transfer.message());
            requestNetworkStop();
        }
    }
    // Spec04 Section3: one quiet checkpoint per completed cleanup, not hot-loop tracing.
    static uint32_t sampledClosure = 0;
    const uint32_t closed = closedAt.load();
    if (!networkStopping.load() && !server && !bleClientConnected() && closed && closed != sampledClosure &&
        ota::elapsed(millis(), closed, 5000)) {
        logOtaHeap("closed-settled");
        sampledClosure = closed;
    }
#ifdef PICKLE_OTA_DIAGNOSTICS
    // Spec04 §9: start a new quiet epoch on closure or any BLE connection/disconnection. Do not defer activation.
    static uint32_t quietSince = 0, lastActivity = 0, lastClosure = 0;
    static size_t quietPoint = 0;
    const uint32_t activity = bleActivityCount();
    if (networkStopping.load() || server || bleClientConnected() || activity != lastActivity || closed != lastClosure) {
        quietSince = millis(); quietPoint = 0; lastActivity = activity; lastClosure = closed;
    } else if (closed) {
        constexpr uint32_t quietMs[] = {5000, 60000, 300000};
        const char* const stages[] = {"quiet-5s", "quiet-60s", "quiet-300s"};
        if (quietPoint < 3 && ota::elapsed(millis(), quietSince, quietMs[quietPoint])) {
            const char* stage = stages[quietPoint++];
            Serial.printf("OTA QUIET closed_at=%u ble_activity=%u quiet_ms=%u\n", closed, activity, uint32_t(millis()-quietSince));
            logOtaHeap(stage);
            ota::diag::ownershipCheckpoint(stage);
        }
    }
#endif
    if (startupOK) ++healthyLoops;
    if (pendingBoot && !confirmationDone && startupOK && healthyLoops >= 100 && ota::elapsed(millis(), readyAt, 5000)) {
        const ota::diag::Budget budget("boot-confirmation");
        if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) { failOtaBootValidation(); return; }
        confirmationDone = true; esp_task_wdt_delete(nullptr);
        esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, true);
        Serial.printf("OTA boot confirmed build=%s heap=%u min_heap=%u\n", buildId(), ESP.getFreeHeap(), ESP.getMinFreeHeap());
        logOtaHeap("boot-confirmed");
    }
}
#endif
