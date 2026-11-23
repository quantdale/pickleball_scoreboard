#include <unity.h>
#include "ota_allocation_ledger.h"

using ota::diag::Allocation;
using ota::diag::AllocationLedger;

void setUp() {}
void tearDown() {}

void test_collision_overflow_and_address_reuse()
{
    AllocationLedger<4> trace;
    // Same initial slot. Exercise backshift from a full, wrapped probe chain.
    for (unsigned i = 0; i < 4; ++i) TEST_ASSERT_TRUE(trace.insert({0x0c + i*16, 32+i, {i}}));
    TEST_ASSERT_FALSE(trace.insert({0x4c, 60, {}}));
    TEST_ASSERT_EQUAL_UINT32(1, trace.drops);
    TEST_ASSERT_EQUAL_UINT32(4, trace.peak);
    TEST_ASSERT_EQUAL_UINT32(32, trace.remove(0x0c).bytes);
    TEST_ASSERT_TRUE(trace.insert({0x0c, 64, {0x42}}));
    TEST_ASSERT_EQUAL_UINT32(0x42, trace.remove(0x0c).pc[0]);
    for (unsigned i = 3; i > 0; --i) TEST_ASSERT_EQUAL_UINT32(32+i, trace.remove(0x0c+i*16).bytes);
    TEST_ASSERT_EQUAL_UINT32(0, trace.live);
    TEST_ASSERT_EQUAL_UINT32(10, trace.revision);
}

void test_failed_resize_restores_original_record_and_success_can_move()
{
    AllocationLedger<8> trace;
    TEST_ASSERT_TRUE(trace.insert({0x2000, 44, {0x40081234}}));
    const Allocation old = trace.remove(0x2000);
    TEST_ASSERT_TRUE(trace.insert(old)); // real realloc failed with nonzero size: original still live
    TEST_ASSERT_EQUAL_UINT32(1, trace.live);
    TEST_ASSERT_EQUAL_UINT32(0x40081234, trace.remove(0x2000).pc[0]);
    TEST_ASSERT_TRUE(trace.insert({0x2010, 96, {0x40082234}})); // simulated move
    TEST_ASSERT_EQUAL_UINT32(0, trace.remove(0x2000).address);
    TEST_ASSERT_EQUAL_UINT32(96, trace.remove(0x2010).bytes);
    TEST_ASSERT_EQUAL_UINT32(0, trace.live);
    TEST_ASSERT_EQUAL_UINT32(0, trace.drops);
}

void test_churn_does_not_exhaust_probe_slots()
{
    AllocationLedger<16> trace;
    uintptr_t active[15] = {};
    uint32_t random = 7;
    for (unsigned i = 0; i < 8192; ++i) {
        random = 1664525u * random + 1013904223u;
        const unsigned slot = (random >> 16) % 15;
        if (active[slot]) TEST_ASSERT_EQUAL_UINT32(slot+1, trace.remove(active[slot]).bytes);
        active[slot] = uintptr_t((i+1)*4);
        TEST_ASSERT_TRUE(trace.insert({active[slot], slot+1, {}}));
    }
    for (unsigned i = 0; i < 15; ++i) TEST_ASSERT_EQUAL_UINT32(i+1, trace.remove(active[i]).bytes);
    TEST_ASSERT_EQUAL_UINT32(0, trace.live);
    TEST_ASSERT_EQUAL_UINT32(0, trace.drops);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_collision_overflow_and_address_reuse);
    RUN_TEST(test_failed_resize_restores_original_record_and_success_can_move);
    RUN_TEST(test_churn_does_not_exhaust_probe_slots);
    return UNITY_END();
}
