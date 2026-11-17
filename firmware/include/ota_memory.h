#ifndef PICKLESCORE_OTA_MEMORY_H
#define PICKLESCORE_OTA_MEMORY_H
#include <cstdint>

// Spec04 Sections3/8: retain conservative reserves; history is telemetry, not available memory.
namespace ota {
constexpr uint32_t STARTUP_FREE_HEAP = 100000;
constexpr uint32_t OPERATION_FREE_HEAP = 49152;
constexpr uint32_t EMERGENCY_FREE_HEAP = 32768;
constexpr uint32_t HTTP_STACK_BYTES = 8192;
constexpr uint32_t HTTP_START_FREE_HEAP = OPERATION_FREE_HEAP + HTTP_STACK_BYTES;
constexpr uint32_t FLASH_SECTOR_BYTES = 4096;
struct HeapSample {
    uint32_t freeInternal, minimumInternal, freeInternal8, largest8, largestInternal8;
};
inline bool heapAdmitted(const HeapSample& sample, uint32_t freeFloor, uint32_t blockFloor = 0)
{
    return sample.freeInternal >= freeFloor && sample.largestInternal8 >= blockFloor;
}

// ESP adapter and host fake share the boot-only controller ordering.
template<class Controller>
bool initializeBleOnlyController(Controller& controller)
{
    if (!controller.isIdle()) return false;
    return controller.releaseClassic() && controller.initializeBle() && controller.enableBle();
}
}
#endif
