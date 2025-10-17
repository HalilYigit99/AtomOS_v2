#include <memory/mmio.h>
#include <arch.h>
#include <debug/debug.h>

#define MMIO_PAGE_SIZE 4096u

static inline uintptr_t align_down(uintptr_t value, uintptr_t alignment)
{
    return value & ~(alignment - 1u);
}

static inline uint64_t align_up_u64(uint64_t value, uint64_t alignment)
{
    return (value + alignment - 1ull) & ~(alignment - 1ull);
}

bool mmio_configure_region(uintptr_t phys_start, size_t length)
{
    if (length == 0) {
        return true;
    }

    uintptr_t start = align_down(phys_start, MMIO_PAGE_SIZE);
    uintptr_t end;

    uint64_t end64 = (uint64_t)phys_start + (uint64_t)length;
    end64 = align_up_u64(end64, MMIO_PAGE_SIZE);
    if (end64 > (uint64_t)UINTPTR_MAX) {
        WARN("MMIO: Range [%p, +%zu) exceeds addressable space", (void*)phys_start, length);
        return false;
    }
    end = (uintptr_t)end64;

    if (end <= start) {
        return false;
    }

    size_t span = (size_t)(end - start);

    arch_paging_pat_init();

    /* Best-effort: ensure identity mapping exists with UC attributes. */
    bool mapped = arch_paging_map_with_type(start, start, span, 0, ARCH_PAGING_MT_UC);
    (void)mapped;

    if (!arch_paging_set_memtype(start, span, ARCH_PAGING_MT_UC)) {
        WARN("MMIO: Failed to apply UC attributes for range [%p - %p)", (void*)start, (void*)end);
        return false;
    }

    if (!arch_mtrr_set_range(start, span, ARCH_PAGING_MT_UC)) {
        LOG("MMIO: MTRR programming unavailable for range [%p - %p), continuing", (void*)start, (void*)end);
    }

    return true;
}
