#include <unity.h>
#include "ota_memory.h"
#include <vector>

void setUp() {}
void tearDown() {}
struct Controller {
    bool idle = true, classicReleased = false, bleInitialized = false, bleEnabled = false;
    int failAt = 0;
    std::vector<int> calls;
    bool startDefault() { calls.push_back(0); return true; } // pinned Arduino legacy BTDM path
    bool isIdle() { calls.push_back(1); return idle; }
    bool releaseClassic() { calls.push_back(2); if (failAt == 2) return false; classicReleased = true; return true; }
    bool initializeBle() { calls.push_back(3); if (failAt == 3 || !classicReleased) return false; bleInitialized = true; return true; }
    bool enableBle() { calls.push_back(4); if (failAt == 4 || !bleInitialized) return false; bleEnabled = true; return true; }
};
void test_ble_only_reclaims_unused_classic_before_enabling()
{
    Controller controller;
    TEST_ASSERT_TRUE(ota::initializeBleOnlyController(controller));
    TEST_ASSERT_TRUE(controller.classicReleased);
    TEST_ASSERT_TRUE(controller.bleInitialized);
    TEST_ASSERT_TRUE(controller.bleEnabled);
    TEST_ASSERT_EQUAL_UINT(4, controller.calls.size());
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL_INT(i+1, controller.calls[i]);
}
void test_never_releases_memory_of_a_live_controller()
{
    Controller controller; controller.idle = false;
    TEST_ASSERT_FALSE(ota::initializeBleOnlyController(controller));
    TEST_ASSERT_EQUAL_UINT(1, controller.calls.size());
    TEST_ASSERT_FALSE(controller.classicReleased);
}
void test_initialization_failures_do_not_enable_or_fallback_to_btdm()
{
    for (int fail = 2; fail <= 4; ++fail) {
        Controller controller; controller.failAt = fail;
        TEST_ASSERT_FALSE(ota::initializeBleOnlyController(controller));
        TEST_ASSERT_EQUAL_UINT(fail, controller.calls.size());
        TEST_ASSERT_FALSE(controller.bleEnabled);
        TEST_ASSERT_EQUAL_INT(1, controller.calls[0]);
    }
}
void test_historical_minimum_does_not_reject_current_safe_heap()
{
    const ota::HeapSample sample{60000, 48208, 60000, 16000, 16000};
    TEST_ASSERT_TRUE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::FLASH_SECTOR_BYTES));
}
void test_actual_current_shortage_rejected_without_lowering_floor()
{
    const ota::HeapSample sample{48208, 48000, 48208, 30000, 30000};
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::FLASH_SECTOR_BYTES));
    TEST_ASSERT_EQUAL_UINT(49152, ota::OPERATION_FREE_HEAP);
    TEST_ASSERT_EQUAL_UINT(100000, ota::STARTUP_FREE_HEAP);
    TEST_ASSERT_EQUAL_UINT(32768, ota::EMERGENCY_FREE_HEAP);
}
void test_fragmented_byte_heap_rejected_despite_total_free()
{
    ota::HeapSample sample{120000, 110000, 110000, 4095, 4095};
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::FLASH_SECTOR_BYTES));
    sample.largest8 = 100000; // external/free8 regions cannot substitute for an INTERNAL block
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::FLASH_SECTOR_BYTES));
}
void test_exact_admission_boundaries_and_http_stack()
{
    ota::HeapSample sample{49152, 1, 49152, 8192, 8192};
    TEST_ASSERT_TRUE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::HTTP_STACK_RESERVE));
    --sample.freeInternal;
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::HTTP_STACK_RESERVE));
    sample.freeInternal = 49152; --sample.largestInternal8;
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::HTTP_STACK_RESERVE));
    TEST_ASSERT_TRUE(ota::heapAdmitted(sample, ota::OPERATION_FREE_HEAP, ota::FLASH_SECTOR_BYTES));
}
void test_http_initialization_reserves_stack_without_spending_operation_floor()
{
    ota::HeapSample sample{57344, 1, 57344, 8192, 8192};
    TEST_ASSERT_EQUAL_UINT(57344, ota::HTTP_START_FREE_HEAP);
    TEST_ASSERT_TRUE(ota::heapAdmitted(sample, ota::HTTP_START_FREE_HEAP, ota::HTTP_STACK_RESERVE));
    --sample.freeInternal;
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::HTTP_START_FREE_HEAP, ota::HTTP_STACK_RESERVE));
    sample.freeInternal = 57344; --sample.largestInternal8;
    TEST_ASSERT_FALSE(ota::heapAdmitted(sample, ota::HTTP_START_FREE_HEAP, ota::HTTP_STACK_RESERVE));
}
void test_smaller_http_stack_keeps_the_original_reserve()
{
    // Spec04 §3: the 7 KiB task keeps the 8 KiB admission reserve and both floors.
    TEST_ASSERT_EQUAL_UINT(8192, ota::HTTP_STACK_RESERVE);
    TEST_ASSERT_EQUAL_UINT(7168, ota::HTTP_STACK_BYTES);
    TEST_ASSERT_EQUAL_UINT(49152, ota::OPERATION_FREE_HEAP);
    TEST_ASSERT_EQUAL_UINT(32768, ota::EMERGENCY_FREE_HEAP);
}
int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_http_initialization_reserves_stack_without_spending_operation_floor);
    RUN_TEST(test_smaller_http_stack_keeps_the_original_reserve);
    RUN_TEST(test_ble_only_reclaims_unused_classic_before_enabling);
    RUN_TEST(test_never_releases_memory_of_a_live_controller);
    RUN_TEST(test_initialization_failures_do_not_enable_or_fallback_to_btdm);
    RUN_TEST(test_historical_minimum_does_not_reject_current_safe_heap);
    RUN_TEST(test_actual_current_shortage_rejected_without_lowering_floor);
    RUN_TEST(test_fragmented_byte_heap_rejected_despite_total_free);
    RUN_TEST(test_exact_admission_boundaries_and_http_stack);
    return UNITY_END();
}
