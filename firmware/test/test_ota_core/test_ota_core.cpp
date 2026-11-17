#include <unity.h>
#include "ota_core.h"
#include "ota_credentials.h"
#include <vector>
#include <algorithm>
#include <limits>

using namespace ota;
void setUp() {}
void tearDown() {}

static Slots layout(unsigned running = 0)
{ return {running ? 0x200000u : 0x10000u, SLOT_SIZE, running ? 0x10000u : 0x200000u, SLOT_SIZE, 0x400000}; }
static PackageHeader package()
{
    PackageHeader h{}; std::memcpy(h.magic, "PKSOTA01", 8); h.format = 1; h.imageSize = 320;
    std::memcpy(h.target, TARGET, sizeof(h.target)); std::memcpy(h.build, "ota-v1-test", 12); h.signatureLength = 8;
    return h; // cryptography is exercised separately with real OpenSSL signatures
}
static std::vector<uint8_t> payload()
{
    std::vector<uint8_t> p(320, 0); p[0] = 0xe9; p[1] = 4; p[23] = 1; return p;
}
struct FakeFlash : Sink {
    int failAt = 0, begins = 0, writes = 0, finishes = 0, verifies = 0, selects = 0, cancels = 0;
    unsigned running = 0, selected = 0;
    std::vector<uint8_t> slots[2];
    FakeFlash() { slots[0].assign(320, 0x5a); }
    bool begin(const PackageHeader&) override { ++begins; if (failAt == 1) return false; slots[1-running].clear(); return true; }
    bool write(const uint8_t* p, size_t n) override { ++writes; if (failAt == 2) return false; slots[1-running].insert(slots[1-running].end(), p, p+n); return true; }
    bool finish() override { ++finishes; return failAt != 3; }
    bool verifyWritten() override { ++verifies; return failAt != 4; }
    bool select() override { ++selects; if (failAt == 5) return false; selected = 1-running; return true; }
    void cancel() override { ++cancels; }
};
static bool stage(Transfer& t, const PackageHeader& h = package(), const Slots& s = layout())
{
    auto bytes = payload();
    return t.begin(h, sizeof(h)+h.imageSize, s, true) && t.write(bytes.data(), bytes.size()) && t.finish();
}

void test_window_timeout_does_not_extend_on_unauthenticated_requests()
{
    Policy p; p.start(10);
    for (uint32_t n = 10; n < WINDOW_MS; n += 1000) p.tick(n, false);
    p.tick(WINDOW_MS+10, false); TEST_ASSERT_TRUE(p.state() == Phase::Closed);
    TEST_ASSERT_FALSE(p.login(WINDOW_MS+11, true, true));
}
void test_failed_auth_limit_and_no_session_before_exclusivity()
{
    Policy p; p.start(0); TEST_ASSERT_FALSE(p.login(1, true, false));
    for (int i = 0; i < 5; ++i) TEST_ASSERT_FALSE(p.login(2+i, false, true));
    TEST_ASSERT_TRUE(p.state() == Phase::Closed);
}
void test_absolute_session_and_transfer_limits()
{
    Policy p; p.start(0); TEST_ASSERT_TRUE(p.login(100, true, true));
    TEST_ASSERT_TRUE(p.begin(101)); p.tick(TRANSFER_MS+101, false);
    TEST_ASSERT_TRUE(p.state() == Phase::Session);
    TEST_ASSERT_TRUE(p.begin(TRANSFER_MS+102)); p.failed();
    p.tick(SESSION_MS+100, false); TEST_ASSERT_TRUE(p.state() == Phase::Closed);
}
void test_millis_wrap_and_ble_priority_in_every_state()
{
    Policy p; uint32_t start = std::numeric_limits<uint32_t>::max()-100;
    p.start(start); p.tick(start+WINDOW_MS, false); TEST_ASSERT_TRUE(p.state() == Phase::Closed);
    for (int state = 0; state < 5; ++state) {
        p.start(0);
        if (state > 0) TEST_ASSERT_TRUE(p.login(1, true, true));
        if (state > 1) TEST_ASSERT_TRUE(p.begin(2));
        if (state > 2) TEST_ASSERT_TRUE(p.transferred(3));
        if (state > 3) TEST_ASSERT_TRUE(p.install(4, true));
        p.tick(5, true); TEST_ASSERT_TRUE(p.state() == Phase::Closed);
    }
}
void test_explicit_install_only_from_verified_stage()
{
    Policy p; p.start(0); p.login(1, true, true);
    TEST_ASSERT_FALSE(p.install(2, true)); p.begin(3);
    TEST_ASSERT_FALSE(p.begin(4)); TEST_ASSERT_FALSE(p.install(5, true));
    TEST_ASSERT_TRUE(p.transferred(6)); TEST_ASSERT_FALSE(p.install(7, false));
    TEST_ASSERT_TRUE(p.install(8, true)); TEST_ASSERT_FALSE(p.install(9, true));
}
void test_partition_selection_and_size_fences()
{
    auto h = package(); TEST_ASSERT_NULL(headerError(h, sizeof(h)+h.imageSize, layout()));
    TEST_ASSERT_NULL(headerError(h, sizeof(h)+h.imageSize, layout(1)));
    auto s = layout(); s.next = s.running; TEST_ASSERT_NOT_NULL(headerError(h, sizeof(h)+h.imageSize, s));
    s = layout(); s.nextSize = 0x140000; TEST_ASSERT_FALSE(validSlots(s));
    s = layout(); s.flashSize = 0x200000; TEST_ASSERT_FALSE(validSlots(s));
    h.imageSize = SLOT_SIZE+1; TEST_ASSERT_NOT_NULL(headerError(h, sizeof(h)+h.imageSize, layout()));
    h.imageSize = std::numeric_limits<uint32_t>::max(); TEST_ASSERT_NOT_NULL(headerError(h, uint64_t(sizeof(h))+h.imageSize, layout()));
}
void test_package_metadata_and_canonical_padding()
{
    const auto clean = package();
    for (int mutation = 0; mutation < 8; ++mutation) {
        auto h = clean;
        if (mutation == 0) h.magic[0] = 'X';
        if (mutation == 1) h.format = 2;
        if (mutation == 2) h.target[0] = 'X';
        if (mutation == 3) h.build[31] = 'a';
        if (mutation == 4) h.build[0] = '<';
        if (mutation == 5) h.signatureLength = 73;
        if (mutation == 6) h.signature[71] = 1;
        if (mutation == 7) h.imageSize = 20;
        TEST_ASSERT_NOT_NULL(headerError(h, sizeof(h)+h.imageSize, layout()));
    }
    TEST_ASSERT_NOT_NULL(headerError(clean, sizeof(clean)+clean.imageSize-1, layout()));
}
void test_untrusted_signature_never_erases_or_selects()
{
    FakeFlash f; Transfer t(f); auto h = package();
    TEST_ASSERT_FALSE(t.begin(h, sizeof(h)+h.imageSize, layout(), false));
    TEST_ASSERT_EQUAL(0, f.begins); TEST_ASSERT_EQUAL(0, f.selects);
    TEST_ASSERT_EQUAL(0x5a, f.slots[0][0]);
}
void test_invalid_prefix_rejected_before_flash_begin()
{
    for (int mutation = 0; mutation < 4; ++mutation) {
        FakeFlash f; Transfer t(f); auto h = package(); auto bytes = payload();
        if (mutation == 0) bytes[0] = 0;
        if (mutation == 1) bytes[12] = 2;
        if (mutation == 2) bytes[1] = 17;
        if (mutation == 3) bytes[23] = 0;
        TEST_ASSERT_TRUE(t.begin(h, sizeof(h)+h.imageSize, layout(), true));
        TEST_ASSERT_FALSE(t.write(bytes.data(), bytes.size())); TEST_ASSERT_EQUAL(0, f.begins);
    }
}
void test_every_small_chunk_boundary_stages_without_boot_selection()
{
    for (size_t chunk = 1; chunk <= 65; ++chunk) {
        FakeFlash f; Transfer t(f); auto h = package(); auto bytes = payload();
        TEST_ASSERT_TRUE(t.begin(h, sizeof(h)+h.imageSize, layout(), true));
        for (size_t offset = 0; offset < bytes.size(); offset += chunk)
            TEST_ASSERT_TRUE(t.write(bytes.data()+offset, std::min(chunk, bytes.size()-offset)));
        TEST_ASSERT_TRUE(t.finish()); TEST_ASSERT_TRUE(t.ready());
        TEST_ASSERT_EQUAL(0, f.selects); TEST_ASSERT_EQUAL_MEMORY(bytes.data(), f.slots[1].data(), bytes.size());
        TEST_ASSERT_EQUAL(0x5a, f.slots[0][0]);
    }
}
void test_truncated_oversized_and_interrupted_images_never_select()
{
    for (int failure = 0; failure < 3; ++failure) {
        FakeFlash f; Transfer t(f); auto h = package(); auto bytes = payload();
        TEST_ASSERT_TRUE(t.begin(h, sizeof(h)+h.imageSize, layout(), true));
        TEST_ASSERT_TRUE(t.write(bytes.data(), 100));
        if (failure == 0) TEST_ASSERT_FALSE(t.finish());
        if (failure == 1) TEST_ASSERT_FALSE(t.write(bytes.data(), bytes.size()));
        if (failure == 2) t.cancel();
        TEST_ASSERT_FALSE(t.ready()); TEST_ASSERT_EQUAL(0, f.selects); TEST_ASSERT_EQUAL(0x5a, f.slots[0][0]);
    }
}
void test_duplicate_request_does_not_destroy_existing_transfer()
{
    FakeFlash f; Transfer t(f); auto h = package(); auto bytes = payload();
    TEST_ASSERT_TRUE(t.begin(h, sizeof(h)+h.imageSize, layout(), true));
    TEST_ASSERT_FALSE(t.begin(h, sizeof(h)+h.imageSize, layout(), true));
    TEST_ASSERT_TRUE(t.write(bytes.data(), bytes.size())); TEST_ASSERT_TRUE(t.finish());
    TEST_ASSERT_FALSE(t.begin(h, sizeof(h)+h.imageSize, layout(), true)); TEST_ASSERT_TRUE(t.ready());
}
void test_flash_failures_leave_running_image_and_no_boot_selection()
{
    for (int failure = 1; failure <= 4; ++failure) {
        FakeFlash f; f.failAt = failure; Transfer t(f);
        TEST_ASSERT_FALSE(stage(t)); TEST_ASSERT_FALSE(t.ready());
        TEST_ASSERT_EQUAL(0, f.selects); TEST_ASSERT_EQUAL(0x5a, f.slots[0][0]);
    }
}
void test_install_requires_authorization_quiet_and_successful_selection()
{
    FakeFlash f; Transfer t(f); TEST_ASSERT_TRUE(stage(t));
    TEST_ASSERT_FALSE(t.install(false, true)); TEST_ASSERT_FALSE(t.install(true, false));
    TEST_ASSERT_EQUAL(0, f.selects); f.failAt = 5;
    TEST_ASSERT_FALSE(t.install(true, true)); TEST_ASSERT_EQUAL(0, f.selected);
}
void test_repeated_updates_alternate_only_inactive_slots()
{
    FakeFlash f;
    for (unsigned i = 0; i < 8; ++i) {
        Transfer t(f); const unsigned before = f.running;
        TEST_ASSERT_TRUE(stage(t, package(), layout(before)));
        TEST_ASSERT_TRUE(t.install(true, true)); TEST_ASSERT_EQUAL(1-before, f.selected);
        f.running = f.selected; // simulated confirmed reboot, NOT real bootloader execution
    }
    TEST_ASSERT_EQUAL(8, f.selects);
}
void test_secret_comparison_and_provisioning_shape_fail_closed()
{
    TEST_ASSERT_TRUE(equalSecret("0123", "0123", 4)); TEST_ASSERT_FALSE(equalSecret("0123", "0124", 4));
    Credentials c{}; TEST_ASSERT_FALSE(credentialShapeValid(c));
    c.schema = 1; std::memcpy(c.ssid, "PickleScore-Court1", 18); std::memset(c.apPassword, 'A', 32);
    c.adminHash[0] = 1; c.publicKey[0] = 4; TEST_ASSERT_TRUE(credentialShapeValid(c));
    c.apPassword[31] = 0; TEST_ASSERT_FALSE(credentialShapeValid(c));
    c.apPassword[31] = 'A'; c.ssid[0] = '<'; TEST_ASSERT_FALSE(credentialShapeValid(c));
}
void test_exact_host_origin_and_path_prevent_bypasses()
{
    TEST_ASSERT_TRUE(localRequestAllowed("/api/upload", "/api/upload", "192.168.4.1", "http://192.168.4.1", true));
    TEST_ASSERT_TRUE(localRequestAllowed("/api/upload", "/api/upload", "192.168.4.1:80", "http://192.168.4.1", true));
    for (const char* uri : {"/api/upload?x=1", "/api/%75pload", "/api/upload/", "/../api/upload", "/API/upload"})
        TEST_ASSERT_FALSE(localRequestAllowed(uri, "/api/upload", "192.168.4.1", "http://192.168.4.1", true));
    for (const char* host : {"evil.example", "192.168.4.1.evil", "192.168.4.1:8080", ""})
        TEST_ASSERT_FALSE(localRequestAllowed("/api/upload", "/api/upload", host, "http://192.168.4.1", true));
    for (const char* origin : {"null", "https://192.168.4.1", "http://evil.example", "http://192.168.4.1:8080", ""})
        TEST_ASSERT_FALSE(localRequestAllowed("/api/upload", "/api/upload", "192.168.4.1", origin, true));
}
void test_authorization_nonce_consumption_and_replay()
{
    SessionAuth auth; char token[65], nonce[65], other[65], bearer[72];
    std::memset(token, 'a', 64); token[64] = 0;
    std::memset(nonce, 'b', 64); nonce[64] = 0;
    std::memset(other, 'c', 64); other[64] = 0;
    std::memcpy(bearer, "Bearer ", 7); std::memcpy(bearer+7, token, 65);
    TEST_ASSERT_FALSE(auth.authorize(bearer, nonce, true));
    auth.issue(token, nonce);
    TEST_ASSERT_FALSE(auth.authorize("", nonce, true));
    TEST_ASSERT_FALSE(auth.authorize(bearer, "", true));
    TEST_ASSERT_FALSE(auth.authorize(bearer, other, true));
    TEST_ASSERT_TRUE(auth.authorize(bearer, "", false)); // authenticated GET status does not consume
    TEST_ASSERT_TRUE(auth.authorize(bearer, nonce, true));
    TEST_ASSERT_FALSE(auth.authorize(bearer, nonce, true)); // replay consumed even if a handler fails
    auth.rotate(other);
    TEST_ASSERT_FALSE(auth.authorize(bearer, nonce, true));
    TEST_ASSERT_TRUE(auth.authorize(bearer, other, true));
    auth.clear(); TEST_ASSERT_FALSE(auth.authorize(bearer, other, false));
    auth.issue(other, nonce); TEST_ASSERT_FALSE(auth.authorize(bearer, nonce, true)); // previous session
}
void test_pinned_sdk_dynamic_tx_cache_is_nonzero_and_bounded()
{
    // Actual Arduino WiFiGeneric.cpp dynamic-TX configuration: cache_tx_buf_num=4, "can't be zero".
    TEST_ASSERT_GREATER_THAN_INT(0, WIFI_TX_CACHE_BUFFERS);
    TEST_ASSERT_LESS_OR_EQUAL_INT(4, WIFI_TX_CACHE_BUFFERS);
}
int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_pinned_sdk_dynamic_tx_cache_is_nonzero_and_bounded);
    RUN_TEST(test_window_timeout_does_not_extend_on_unauthenticated_requests);
    RUN_TEST(test_failed_auth_limit_and_no_session_before_exclusivity);
    RUN_TEST(test_absolute_session_and_transfer_limits);
    RUN_TEST(test_millis_wrap_and_ble_priority_in_every_state);
    RUN_TEST(test_explicit_install_only_from_verified_stage);
    RUN_TEST(test_partition_selection_and_size_fences);
    RUN_TEST(test_package_metadata_and_canonical_padding);
    RUN_TEST(test_untrusted_signature_never_erases_or_selects);
    RUN_TEST(test_invalid_prefix_rejected_before_flash_begin);
    RUN_TEST(test_every_small_chunk_boundary_stages_without_boot_selection);
    RUN_TEST(test_truncated_oversized_and_interrupted_images_never_select);
    RUN_TEST(test_duplicate_request_does_not_destroy_existing_transfer);
    RUN_TEST(test_flash_failures_leave_running_image_and_no_boot_selection);
    RUN_TEST(test_install_requires_authorization_quiet_and_successful_selection);
    RUN_TEST(test_repeated_updates_alternate_only_inactive_slots);
    RUN_TEST(test_secret_comparison_and_provisioning_shape_fail_closed);
    RUN_TEST(test_exact_host_origin_and_path_prevent_bypasses);
    RUN_TEST(test_authorization_nonce_consumption_and_replay);
    return UNITY_END();
}
