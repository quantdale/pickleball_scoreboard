#include <unity.h>
#include "ble_activation_core.h"
#include <cstring>

uint8_t challenge[39], proof[34], digest[32];
ota::ActivationGate gate;
void setUp() {
    gate = ota::ActivationGate();
    std::memset(challenge, 0x37, sizeof(challenge)); challenge[0] = 1;
    std::memset(digest, 0x52, sizeof(digest));
    proof[0] = proof[1] = 1; std::memcpy(proof+2, digest, 32);
}
void tearDown() {}
void accepts_one_current_proof() {
    TEST_ASSERT_TRUE(gate.issue(100, challenge));
    TEST_ASSERT_EQUAL_MEMORY(challenge, gate.bytes(), 39);
    TEST_ASSERT_TRUE(gate.accept(101, proof, 34, digest));
    TEST_ASSERT_FALSE(gate.accept(102, proof, 34, digest));
}
void expires_challenge() {
    gate.issue(100, challenge);
    TEST_ASSERT_FALSE(gate.accept(20100, proof, 34, digest));
}
void wrong_digest_consumes() {
    gate.issue(0, challenge); proof[2] ^= 1;
    TEST_ASSERT_FALSE(gate.accept(1, proof, 34, digest)); proof[2] ^= 1;
    TEST_ASSERT_FALSE(gate.accept(2, proof, 34, digest));
}
void requires_disruption_authorization() {
    gate.issue(0, challenge); proof[1] = 0;
    TEST_ASSERT_FALSE(gate.accept(1, proof, 34, digest));
}
void rejects_wrong_version_and_length() {
    for (size_t size = 0; size < 34; ++size) {
        gate.issue(0, challenge); TEST_ASSERT_FALSE(gate.accept(1, proof, size, digest));
    }
    gate.issue(0, challenge); proof[0] = 2;
    TEST_ASSERT_FALSE(gate.accept(1, proof, 34, digest));
}
void disconnect_invalidates() {
    gate.issue(0, challenge); gate.invalidate();
    TEST_ASSERT_FALSE(gate.accept(1, proof, 34, digest));
}
void five_failures_rate_limit_across_connections() {
    proof[2] ^= 1;
    for (unsigned i = 0; i < 5; ++i) {
        TEST_ASSERT_TRUE(gate.issue(i, challenge));
        TEST_ASSERT_FALSE(gate.accept(i, proof, 34, digest));
    }
    proof[2] ^= 1;
    TEST_ASSERT_FALSE(gate.issue(1000, challenge));
    TEST_ASSERT_FALSE(gate.accept(1001, proof, 34, digest));
    TEST_ASSERT_TRUE(gate.issue(62000, challenge));
    TEST_ASSERT_TRUE(gate.accept(62001, proof, 34, digest));
}
void expiry_wrap_safe() {
    gate.issue(0xfffffff0, challenge);
    TEST_ASSERT_TRUE(gate.accept(20, proof, 34, digest));
    gate.issue(0xfffffff0, challenge);
    TEST_ASSERT_FALSE(gate.accept(20000, proof, 34, digest));
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(accepts_one_current_proof); RUN_TEST(expires_challenge);
    RUN_TEST(wrong_digest_consumes); RUN_TEST(requires_disruption_authorization);
    RUN_TEST(rejects_wrong_version_and_length); RUN_TEST(disconnect_invalidates);
    RUN_TEST(five_failures_rate_limit_across_connections); RUN_TEST(expiry_wrap_safe);
    return UNITY_END();
}
