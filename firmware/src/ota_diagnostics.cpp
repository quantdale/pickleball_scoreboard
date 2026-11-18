#ifdef PICKLE_OTA_DIAGNOSTICS
#include "ota_diagnostics.h"
#include "ota_allocation_ledger.h"
#include <Arduino.h>
#include <atomic>
#include <cstdio>
#include <esp_debug_helpers.h>
#include <esp_heap_caps.h>
#include <esp_spi_flash.h>
#include <multi_heap.h>
#include <soc/cpu.h>

namespace {
// Spec04 §§3–5/8: fixed-size telemetry, no allocation or logging inside allocator hooks.
std::atomic<bool> watching{false};
portMUX_TYPE budgetMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t epoch = 0, minimum = UINT32_MAX, minimum8 = UINT32_MAX, samples = 0;
bool active = false, initialized = false; // guarded by budgetMux
std::atomic<uint32_t> skipped{0};
bool IRAM_ATTR safeContext() { return !xPortInIsrContext() && spi_flash_cache_enabled(); }

void IRAM_ATTR sampleAllocation(void* result)
{
    if (!result || !watching.load(std::memory_order_relaxed)) return;
    if (!safeContext()) { skipped.fetch_add(1, std::memory_order_relaxed); return; }
    portENTER_CRITICAL(&budgetMux);
    const uint32_t generation = epoch;
    const bool sampling = active;
    portEXIT_CRITICAL(&budgetMux);
    if (!sampling) return;
    // Pinned SDK: get_free_size walks registered heaps, not individual blocks.
    const uint32_t current = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const uint32_t current8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    portENTER_CRITICAL(&budgetMux);
    if (epoch == generation) {
        if (current < minimum) minimum = current;
        if (current8 < minimum8) minimum8 = current8;
        ++samples;
    }
    portEXIT_CRITICAL(&budgetMux);
}

#ifdef PICKLE_OTA_OWNERSHIP
constexpr size_t Capacity = 256;
ota::diag::AllocationLedger<Capacity> ledger;
portMUX_TYPE ledgerMux = portMUX_INITIALIZER_UNLOCKED;
std::atomic<bool> tracing{false};
std::atomic<uint32_t> traceSkipped{0};

// Print::printf would allocate and invalidate our own snapshot revision.
template<class... Fields>
void reportOwnership(const char* format, Fields... fields)
{
    char line[160];
    const int n = std::snprintf(line, sizeof(line), format, fields...);
    if (n > 0 && size_t(n) < sizeof(line)) Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(n));
}

void IRAM_ATTR remember(void* address, size_t size)
{
    if (!address || !tracing.load(std::memory_order_relaxed)) return;
    if (!safeContext()) { traceSkipped.fetch_add(1, std::memory_order_relaxed); return; }
    ota::diag::Allocation record{reinterpret_cast<uintptr_t>(address), uint32_t(size), {}};
    esp_backtrace_frame_t frame{};
    esp_backtrace_get_start(&frame.pc, &frame.sp, &frame.next_pc);
    // Start frame is the wrapper; the following PCs expose caps allocator and owning caller(s).
    for (auto& pc : record.pc) {
        if (!frame.next_pc || !esp_backtrace_get_next_frame(&frame)) break;
        pc = esp_cpu_process_stack_pc(frame.pc);
    }
    portENTER_CRITICAL(&ledgerMux); ledger.insert(record); portEXIT_CRITICAL(&ledgerMux);
}
ota::diag::Allocation IRAM_ATTR forget(void* address)
{
    if (!address || !tracing.load(std::memory_order_relaxed)) return {};
    if (!safeContext()) { traceSkipped.fetch_add(1, std::memory_order_relaxed); return {}; }
    portENTER_CRITICAL(&ledgerMux);
    const auto old = ledger.remove(reinterpret_cast<uintptr_t>(address));
    portEXIT_CRITICAL(&ledgerMux);
    return old;
}
#endif
}

extern "C" void* __real_multi_heap_malloc(multi_heap_handle_t, size_t);
extern "C" void* IRAM_ATTR __wrap_multi_heap_malloc(multi_heap_handle_t heap, size_t size)
{
    void* result = __real_multi_heap_malloc(heap, size);
#ifdef PICKLE_OTA_OWNERSHIP
    remember(result, size);
#endif
    sampleAllocation(result);
    return result;
}
extern "C" void* __real_multi_heap_aligned_alloc(multi_heap_handle_t, size_t, size_t);
extern "C" void* IRAM_ATTR __wrap_multi_heap_aligned_alloc(multi_heap_handle_t heap, size_t size, size_t alignment)
{
    void* result = __real_multi_heap_aligned_alloc(heap, size, alignment);
#ifdef PICKLE_OTA_OWNERSHIP
    remember(result, size);
#endif
    sampleAllocation(result);
    return result;
}
extern "C" void* __real_multi_heap_realloc(multi_heap_handle_t, void*, size_t);
extern "C" void* IRAM_ATTR __wrap_multi_heap_realloc(multi_heap_handle_t heap, void* address, size_t size)
{
#ifdef PICKLE_OTA_OWNERSHIP
    const auto old = forget(address); // old address may be reused on the other core after realloc
#endif
    void* result = __real_multi_heap_realloc(heap, address, size);
#ifdef PICKLE_OTA_OWNERSHIP
    if (result) remember(result, size);
    else if (size && old.address && safeContext()) {
        portENTER_CRITICAL(&ledgerMux); ledger.insert(old); portEXIT_CRITICAL(&ledgerMux);
    }
#endif
    sampleAllocation(result);
    return result;
}
#ifdef PICKLE_OTA_OWNERSHIP
extern "C" void __real_multi_heap_free(multi_heap_handle_t, void*);
extern "C" void IRAM_ATTR __wrap_multi_heap_free(multi_heap_handle_t heap, void* address)
{
    forget(address);
    __real_multi_heap_free(heap, address);
}
#endif

namespace ota { namespace diag {
void startOwnership()
{
#ifdef PICKLE_OTA_OWNERSHIP
    tracing.store(true, std::memory_order_relaxed);
#endif
}
void ownershipCheckpoint(const char* stage)
{
#ifdef PICKLE_OTA_OWNERSHIP
    portENTER_CRITICAL(&ledgerMux);
    const uint32_t live = ledger.live, peak = ledger.peak, drops = ledger.drops, revision = ledger.revision;
    portEXIT_CRITICAL(&ledgerMux);
    const uint32_t missed = traceSkipped.load(std::memory_order_relaxed);
    reportOwnership("OTA OWN begin=%s live=%u peak=%u drops=%u skipped=%u rev=%u\n", stage, live, peak, drops, missed, revision);
    for (size_t i = 0; i < Capacity; ++i) {
        portENTER_CRITICAL(&ledgerMux); const auto r = ledger.entry(i); portEXIT_CRITICAL(&ledgerMux);
        if (r.address) reportOwnership("OTA OWN ptr=%08x bytes=%u pc=%08x,%08x,%08x,%08x\n",
            unsigned(r.address), r.bytes, r.pc[0], r.pc[1], r.pc[2], r.pc[3]);
    }
    portENTER_CRITICAL(&ledgerMux); const uint32_t end = ledger.revision; portEXIT_CRITICAL(&ledgerMux);
    reportOwnership("OTA OWN end=%s rev=%u skipped=%u\n", stage, end, traceSkipped.load(std::memory_order_relaxed));
#else
    (void)stage;
#endif
}
static void reportBudget(const char* stage, const char* edge)
{
    const auto stack = uxTaskGetStackHighWaterMark(nullptr);
    const auto free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const auto free8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    const auto block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    portENTER_CRITICAL(&budgetMux);
    if (active) {
        if (free < minimum) minimum = free;
        if (free8 < minimum8) minimum8 = free8;
    }
    const bool valid = initialized;
    const uint32_t low = minimum, low8 = minimum8, count = samples;
    portEXIT_CRITICAL(&budgetMux);
    // Print::printf uses a 64-byte local buffer then mallocs; avoid measuring our own print allocations.
    char line[256];
    const int n = std::snprintf(line, sizeof(line), "OTA BUDGET stage=%s edge=%s task=%s stack_free=%u free=%u free8=%u largest8=%u low=%u low8=%u samples=%u skipped=%u\n",
        stage, edge, pcTaskGetName(nullptr), unsigned(stack), unsigned(free), unsigned(free8), unsigned(block),
        valid ? low : 0, valid ? low8 : 0, count, skipped.load(std::memory_order_relaxed));
    if (n > 0 && size_t(n) < sizeof(line)) Serial.write(reinterpret_cast<const uint8_t*>(line), size_t(n));
}
void budgetPoint(const char* stage) { reportBudget(stage, "point"); }
bool budgetBegin(const char* stage)
{
    const uint32_t free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const uint32_t free8 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    portENTER_CRITICAL(&budgetMux);
    const bool started = !active;
    if (started) {
        active = initialized = true;
        ++epoch; minimum = free; minimum8 = free8; samples = 0;
        skipped.store(0, std::memory_order_relaxed); watching.store(true, std::memory_order_relaxed);
    }
    portEXIT_CRITICAL(&budgetMux);
    if (started) reportBudget(stage, "begin");
    return started;
}
void budgetEnd(const char* stage)
{
    portENTER_CRITICAL(&budgetMux);
    active = false; watching.store(false, std::memory_order_relaxed); ++epoch;
    portEXIT_CRITICAL(&budgetMux);
    reportBudget(stage, "end");
}
}}
#endif
