// HPET driver for x86/x86_64
#include <arch.h>
#include <time/timer.h>
#include <stdbool.h>
#include <driver/DriverBase.h>
#include <acpi/acpi.h>
#include <acpi/acpi_new.h>
#include <irq/IRQ.h>
#include <debug/debug.h>
#include <list.h>

// ---- HPET registers & helpers ----
#define HPET_REG_CAP_ID      0x000ull // General Capabilities and ID (RO)
#define HPET_REG_CONFIG      0x010ull // General Configuration (RW)
#define HPET_REG_ISR         0x020ull // General Interrupt Status (RW1C)
#define HPET_REG_MAIN_CNT    0x0F0ull // Main Counter (RW)

#define HPET_TN_CFG(n)       (0x100ull + 0x20ull * (uint64_t)(n))
#define HPET_TN_CMP(n)       (0x108ull + 0x20ull * (uint64_t)(n))
#define HPET_TN_FSB(n)       (0x110ull + 0x20ull * (uint64_t)(n))

// CAP bits
#define HPET_CAP_LEG_RT_CAP  (1ull << 15)
#define HPET_CAP_CNT_SIZE    (1ull << 13)
#define HPET_CAP_NUM_TIMERS(x) ((((x) >> 8) & 0x1F) + 1)
#define HPET_CAP_CLK_PERIOD(x) ((uint32_t)((x) >> 32)) // femtoseconds

// CONFIG bits
#define HPET_CFG_ENABLE      (1ull << 0)
#define HPET_CFG_LEG_RT_CNF  (1ull << 1)

// Timer N config bits (lower dword)
#define HPET_TN_INT_TYPE_LVL (1u << 1)  // 1=level, 0=edge
#define HPET_TN_INT_ENB      (1u << 2)
#define HPET_TN_TYPE_PERIOD  (1u << 3)
#define HPET_TN_PER_CAP      (1u << 4)  // RO: periodic capable
#define HPET_TN_SIZE_CAP     (1u << 5)  // RO: 64-bit capable
#define HPET_TN_VAL_SET      (1u << 6)  // write 1 before setting accumulator in periodic mode
#define HPET_TN_32MODE       (1u << 8)  // 1=32-bit comparator mode
#define HPET_TN_INT_ROUTE_SHIFT 9       // routing field start

// We use comparator 0 for system tick
#define HPET_TIMER_INDEX 0
#define HPET_IRQ_LEGACY  0  // Legacy replacement routes timer0 as IRQ0 logically

static volatile uint64_t* s_hpet_mmio = NULL; // identity-mapped
static uint32_t s_hpet_period_fs = 0;         // femtoseconds per tick
static uint64_t s_hpet_counter_hz = 0;        // derived: Hz
static bool     s_hpet_running = false;

// Callback list for tick
static List* s_hpet_callbacks = NULL;

// Externs provided by other parts
extern HardwareTimer* hpet_timer; // global pointer in kernel/time/timer.c
extern void hpet_timer_isr();     // ASM stub defined in hpet.asm

static inline uint64_t hpet_read64(uint64_t off)
{
    return *(volatile uint64_t*)((volatile uint8_t*)s_hpet_mmio + off);
}

static inline void hpet_write64(uint64_t off, uint64_t val)
{
    *(volatile uint64_t*)((volatile uint8_t*)s_hpet_mmio + off) = val;
    (void)hpet_read64(HPET_REG_CAP_ID); // post write flush
}

static inline uint64_t hpet_now_ticks(void)
{
    return hpet_read64(HPET_REG_MAIN_CNT);
}

// Convert desired frequency (Hz) to HPET ticks per interrupt
static uint64_t hpet_ticks_for_hz(uint32_t hz)
{
    if (hz == 0 || s_hpet_counter_hz == 0) return 0;
    return (s_hpet_counter_hz + (hz/2)) / hz; // rounded
}

bool hpet_supported()
{
    // Requires ACPI HPET table with System Memory GAS, MMIO address non-zero
    if (!acpi_hpet_ptr) return false;
    const acpi_hpet* hpet = (const acpi_hpet*)acpi_hpet_ptr;
    if (!hpet) return false;
    if (hpet->BaseAddress.AddressSpaceId != 0 /* System Memory */) return false;
    if (hpet->BaseAddress.Address == 0) return false;

    // MMIO assumed identity-mapped in low 4GiB
    volatile uint64_t* base = (volatile uint64_t*)(uintptr_t)hpet->BaseAddress.Address;
    uint64_t cap = *(volatile uint64_t*)((volatile uint8_t*)base + HPET_REG_CAP_ID);
    uint32_t period = HPET_CAP_CLK_PERIOD(cap);
    if (period == 0 || period > 100000000u /* >100ns invalid */) return false;

    // We prefer legacy replacement capable hardware for simplest routing
    bool legacy_cap = (cap & HPET_CAP_LEG_RT_CAP) != 0;
    return legacy_cap; // keep simple for now
}

static void hpet_program_periodic(uint32_t hz)
{
    uint64_t ticks = hpet_ticks_for_hz(hz);
    if (ticks == 0) ticks = 1;

    // Disable main counter while reprogramming
    uint64_t cfg = hpet_read64(HPET_REG_CONFIG);
    cfg &= ~HPET_CFG_ENABLE;
    hpet_write64(HPET_REG_CONFIG, cfg);

    // Clear any pending interrupts (RW1C)
    hpet_write64(HPET_REG_ISR, (1ull << HPET_TIMER_INDEX));

    // Program timer 0: periodic, interrupt enabled, level-triggered is fine
    uint64_t tcfg = hpet_read64(HPET_TN_CFG(HPET_TIMER_INDEX));
    // Set INT_ENB, PERIODIC, and VAL_SET; leave routing as legacy
    tcfg |= (uint64_t)(HPET_TN_INT_ENB | HPET_TN_TYPE_PERIOD | HPET_TN_VAL_SET);
    // Prefer 64-bit comparator if available: clear 32-bit mode
    tcfg &= ~(uint64_t)HPET_TN_32MODE;
    hpet_write64(HPET_TN_CFG(HPET_TIMER_INDEX), tcfg);

    // First write: absolute time (now + delta)
    uint64_t now = hpet_now_ticks();
    hpet_write64(HPET_TN_CMP(HPET_TIMER_INDEX), now + ticks);
    // Second write (after VAL_SET): periodic accumulator interval
    hpet_write64(HPET_TN_CMP(HPET_TIMER_INDEX), ticks);

    // Enable legacy replacement route and main counter
    cfg |= (HPET_CFG_LEG_RT_CNF | HPET_CFG_ENABLE);
    hpet_write64(HPET_REG_CONFIG, cfg);
}

static int hpet_start()
{
    if (!s_hpet_mmio) return -1;
    if (!irq_controller) return -1;
    // Route IRQ0 to our ISR; actual GSI handled by controller (APIC/PIC)
    irq_controller->register_handler(HPET_IRQ_LEGACY, hpet_timer_isr);
    hpet_program_periodic((uint32_t)hpet_timer->frequency);
    irq_controller->enable(HPET_IRQ_LEGACY);
    s_hpet_running = true;
    return 0;
}

static int hpet_stop()
{
    if (!s_hpet_mmio) return -1;
    uint64_t cfg = hpet_read64(HPET_REG_CONFIG);
    cfg &= ~HPET_CFG_ENABLE;
    hpet_write64(HPET_REG_CONFIG, cfg);
    // Mask legacy IRQ0
    if (irq_controller) irq_controller->disable(HPET_IRQ_LEGACY);
    s_hpet_running = false;
    return 0;
}

static int hpet_setFrequency(uint32_t frequency)
{
    if (frequency == 0) return -1;
    hpet_timer->frequency = frequency;
    if (s_hpet_running) hpet_program_periodic(frequency);
    LOG("HPET: Frequency set -> %u Hz (counter=%llu Hz)", frequency, (unsigned long long)s_hpet_counter_hz);
    return 0;
}

static int hpet_setPrescaler(uint32_t prescaler)
{
    (void)prescaler; // Not applicable for HPET
    return -1;
}

void hpet_add_callback(void (*callback)())
{
    if (!callback) return;
    if (!s_hpet_callbacks) s_hpet_callbacks = List_Create();
    List_Add(s_hpet_callbacks, callback);
}

void hpet_remove_callback(void (*callback)())
{
    if (s_hpet_callbacks && callback) List_Remove(s_hpet_callbacks, callback);
}

// C handler called by ASM stub
void hpet_timer_handler()
{
    // Clear timer0 interrupt status (RW1C)
    if (s_hpet_mmio) hpet_write64(HPET_REG_ISR, (1ull << HPET_TIMER_INDEX));

    if (s_hpet_callbacks && s_hpet_callbacks->count > 0) {
        LIST_FOR_EACH(s_hpet_callbacks, it) {
            void (*cb)() = (void (*)())it->data;
            if (cb) cb();
        }
    }

    if (irq_controller) irq_controller->acknowledge(HPET_IRQ_LEGACY);
}

static void hpet_timer_init_wrapper() { /* no-op; start() programs hardware */ }

static bool hpet_init()
{
    if (!acpi_hpet_ptr) {
        LOG("HPET: ACPI table not found");
        return false;
    }

    const acpi_hpet* hpet = (const acpi_hpet*)acpi_hpet_ptr;
    if (hpet->BaseAddress.AddressSpaceId != 0) {
        ERROR("HPET: BaseAddress is not in System Memory (ASID=%u)", hpet->BaseAddress.AddressSpaceId);
        return false;
    }

    s_hpet_mmio = (volatile uint64_t*)(uintptr_t)hpet->BaseAddress.Address;
    if (!s_hpet_mmio) {
        ERROR("HPET: MMIO base is NULL");
        return false;
    }

    uint64_t cap = hpet_read64(HPET_REG_CAP_ID);
    s_hpet_period_fs = HPET_CAP_CLK_PERIOD(cap);
    if (s_hpet_period_fs == 0) {
        ERROR("HPET: invalid clock period");
        return false;
    }
    s_hpet_counter_hz = 1000000000000000ull / (uint64_t)s_hpet_period_fs;
    uint32_t num_timers = HPET_CAP_NUM_TIMERS(cap);
    bool legacy_capable = (cap & HPET_CAP_LEG_RT_CAP) != 0;

    LOG("HPET: base=%p, period=%u fs (~%llu Hz), timers=%u, legacy=%s",
        (void*)s_hpet_mmio, (unsigned)s_hpet_period_fs,
        (unsigned long long)s_hpet_counter_hz, (unsigned)num_timers,
        legacy_capable ? "yes" : "no");

    if (!legacy_capable) {
        ERROR("HPET: Legacy replacement not supported; skipping HPET for now");
        return false;
    }

    // Bind global timer object
    static HardwareTimer hpet_timer_instance;
    hpet_timer = &hpet_timer_instance;
    hpet_timer->name = "HPET";
    hpet_timer->frequency = 1000; // default 1 kHz system tick
    hpet_timer->context = NULL;
    hpet_timer->init = hpet_timer_init_wrapper;
    hpet_timer->start = hpet_start;
    hpet_timer->stop = hpet_stop;
    hpet_timer->setPrescaler = hpet_setPrescaler;
    hpet_timer->setFrequency = hpet_setFrequency;
    hpet_timer->add_callback = hpet_add_callback;
    hpet_timer->remove_callback = hpet_remove_callback;

    // Install ISR but do not unmask yet; start() will enable
    if (irq_controller && irq_controller->register_handler) {
        irq_controller->register_handler(HPET_IRQ_LEGACY, hpet_timer_isr);
    }

    LOG("HPET: Initialized");
    return true;
}

static void hpet_enable()
{
    (void)hpet_start();
    LOG("HPET: enabled");
}

static void hpet_disable()
{
    (void)hpet_stop();
    LOG("HPET: disabled");
}

DriverBase hpet_driver = {
    .name = "HPET",
    .enabled = false,
    .version = 1,
    .context = NULL,
    .init = hpet_init,
    .enable = hpet_enable,
    .disable = hpet_disable,
    .type = DRIVER_TYPE_TIMER
};
