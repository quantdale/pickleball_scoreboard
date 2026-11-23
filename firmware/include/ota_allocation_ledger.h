#ifndef PICKLESCORE_OTA_ALLOCATION_LEDGER_H
#define PICKLESCORE_OTA_ALLOCATION_LEDGER_H
#include <cstddef>
#include <cstdint>

namespace ota { namespace diag {
// Spec04 §8 bench diagnostics. Fixed storage, task/ISR synchronization supplied by caller.
struct Allocation {
    uintptr_t address;
    uint32_t bytes; // requested at multi_heap boundary, not allocator overhead
    uint32_t pc[4];
};

template<size_t Capacity>
class AllocationLedger {
    static_assert(Capacity && !(Capacity & (Capacity-1)), "Power of two required");
public:
    bool insert(Allocation record) {
        if (!record.address) return false;
        const size_t slot = search(record.address);
        if (slot == Capacity || entries[slot].address) { ++drops; return false; }
        entries[slot] = record; ++live; ++revision;
        if (live > peak) peak = live;
        return true;
    }
    Allocation remove(uintptr_t address) {
        if (!address) return {};
        size_t hole = search(address);
        if (hole == Capacity || !entries[hole].address) return {};
        const Allocation old = entries[hole];
        entries[hole].address = 0;
        // Repair the affected probe chain without ever accumulating tombstones.
        for (size_t next = advance(hole), scanned = 1; scanned < Capacity && entries[next].address;
            ++scanned, next = advance(next)) {
            const size_t home = index(entries[next].address);
            if (((hole - home) & Mask) < ((next - home) & Mask)) {
                entries[hole] = entries[next]; entries[next].address = 0; hole = next;
            }
        }
        --live; ++revision;
        return old;
    }
    Allocation entry(size_t index) const { return entries[index]; }
    uint32_t live = 0, peak = 0, drops = 0, revision = 0;
private:
    static constexpr size_t Mask = Capacity-1;
    static size_t advance(size_t i) { return (i+1) & Mask; }
    static size_t index(uintptr_t address) { return ((address >> 2) ^ (address >> 10)) & Mask; }
    size_t search(uintptr_t address) const {
        size_t i = index(address);
        for (size_t probed = 0; probed < Capacity; ++probed, i = advance(i))
            if (!entries[i].address || entries[i].address == address) return i;
        return Capacity;
    }
    Allocation entries[Capacity]{};
};
}}
#endif
