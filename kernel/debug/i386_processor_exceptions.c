/*

    Intel i386/x86_64 exception handling (IDT vector 0-31)

*/

#include <arch.h>
#include <debug/debug.h>
#include <debug/debugTerm.h>
#include <debug/uart.h>
#include <gfxterm/gfxterm.h>
#include <util/VPrintf.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern void i386_exception_0_isr();
extern void i386_exception_1_isr();
extern void i386_exception_2_isr();
extern void i386_exception_3_isr();
extern void i386_exception_4_isr();
extern void i386_exception_5_isr();
extern void i386_exception_6_isr();
extern void i386_exception_7_isr();
extern void i386_exception_8_isr();
extern void i386_exception_9_isr();
extern void i386_exception_10_isr();
extern void i386_exception_11_isr();
extern void i386_exception_12_isr();
extern void i386_exception_13_isr();
extern void i386_exception_14_isr();
extern void i386_exception_15_isr();
extern void i386_exception_16_isr();
extern void i386_exception_17_isr();
extern void i386_exception_18_isr();
extern void i386_exception_19_isr();
extern void i386_exception_20_isr();
extern void i386_exception_21_isr();
extern void i386_exception_22_isr();
extern void i386_exception_23_isr();
extern void i386_exception_24_isr();
extern void i386_exception_25_isr();
extern void i386_exception_26_isr();
extern void i386_exception_27_isr();
extern void i386_exception_28_isr();
extern void i386_exception_29_isr();
extern void i386_exception_30_isr();
extern void i386_exception_31_isr();

extern void acpi_restart();

static const char* const exception_names[32] = {
    "Divide Error",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "BOUND Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 FPU Floating-Point Error",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Virtualization Exception",
    "Control Protection Exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Security Exception",
    "Reserved"
};

static const char* get_exception_name(uint8_t vector)
{
    if (vector < (sizeof(exception_names) / sizeof(exception_names[0])))
        return exception_names[vector];
    return "Unknown";
}

static bool isError(size_t exceptionId)
{
    switch (exceptionId)
    {
    case 1:  // Debug
    case 2:  // Non-Maskable Interrupt
    case 3:  // Breakpoint
    case 4:  // Overflow
    case 5:  // BOUND Range Exceeded
    case 7:  // Device Not Available
    // case 14: // Page Fault (pager could recover)
    case 16: // x87 FPU Floating-Point Error
    case 17: // Alignment Check
    case 19: // SIMD Floating-Point Exception
    case 20: // Virtualization Exception
        return false;
    default:
        return true;
    }
}

typedef struct
{
    bool use_gfx;
    bool use_uart;
} exception_output_ctx_t;

static exception_output_ctx_t* g_active_exception_output = NULL;

static void exception_put_char(char c)
{
    if (!g_active_exception_output)
        return;

    if (g_active_exception_output->use_gfx)
    {
        GFXTerminal* term = debugterm_get();
        if (term)
        {
            gfxterm_putChar(term, c);
        }
    }

    // if (g_active_exception_output->use_uart)
    // {
    //     uart_write_char(c);
    // }
}

static void exception_vprintf(exception_output_ctx_t* ctx, const char* fmt, va_list args)
{
    if (!ctx || !fmt)
        return;

    g_active_exception_output = ctx;
    vprintf(exception_put_char, fmt, args);
    g_active_exception_output = NULL;
}

static void exception_printf(exception_output_ctx_t* ctx, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    exception_vprintf(ctx, fmt, args);
    va_end(args);
}

static exception_output_ctx_t exception_prepare_output(void)
{
    exception_output_ctx_t ctx = {0};

    if (debugterm_is_ready() && debugterm_ensure_ready())
    {
        ctx.use_gfx = true;
    }

    // if (!ctx.use_gfx)
    // {
    //     uart_open();
    //     ctx.use_uart = true;
    // }

    return ctx;
}

static uint16_t read_ds(void)
{
    uint16_t value;
    asm volatile ("mov %%ds, %0" : "=r"(value));
    return value;
}

static uint16_t read_es(void)
{
    uint16_t value;
    asm volatile ("mov %%es, %0" : "=r"(value));
    return value;
}

static uint16_t read_fs(void)
{
    uint16_t value;
    asm volatile ("mov %%fs, %0" : "=r"(value));
    return value;
}

static uint16_t read_gs(void)
{
    uint16_t value;
    asm volatile ("mov %%gs, %0" : "=r"(value));
    return value;
}

static uint16_t read_ss(void)
{
    uint16_t value;
    asm volatile ("mov %%ss, %0" : "=r"(value));
    return value;
}

#if defined(__x86_64__)
static uint64_t read_cr0(void)
{
    uint64_t value;
    asm volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static uint64_t read_cr2(void)
{
    uint64_t value;
    asm volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static uint64_t read_cr3(void)
{
    uint64_t value;
    asm volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static uint64_t read_cr4(void)
{
    uint64_t value;
    asm volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}

static uint64_t read_cr8(void)
{
    uint64_t value;
    asm volatile ("mov %%cr8, %0" : "=r"(value));
    return value;
}
#else
static uint32_t read_cr0(void)
{
    uint32_t value;
    asm volatile ("mov %%cr0, %0" : "=r"(value));
    return value;
}

static uint32_t read_cr2(void)
{
    uint32_t value;
    asm volatile ("mov %%cr2, %0" : "=r"(value));
    return value;
}

static uint32_t read_cr3(void)
{
    uint32_t value;
    asm volatile ("mov %%cr3, %0" : "=r"(value));
    return value;
}

static uint32_t read_cr4(void)
{
    uint32_t value;
    asm volatile ("mov %%cr4, %0" : "=r"(value));
    return value;
}
#endif

void i386_processor_exceptions_init()
{
    idt_set_gate(0, (uintptr_t)i386_exception_0_isr);
    idt_set_gate(1, (uintptr_t)i386_exception_1_isr);
    idt_set_gate(2, (uintptr_t)i386_exception_2_isr);
    idt_set_gate(3, (uintptr_t)i386_exception_3_isr);
    idt_set_gate(4, (uintptr_t)i386_exception_4_isr);
    idt_set_gate(5, (uintptr_t)i386_exception_5_isr);
    idt_set_gate(6, (uintptr_t)i386_exception_6_isr);
    idt_set_gate(7, (uintptr_t)i386_exception_7_isr);
    idt_set_gate(8, (uintptr_t)i386_exception_8_isr);
    idt_set_gate(9, (uintptr_t)i386_exception_9_isr);
    idt_set_gate(10, (uintptr_t)i386_exception_10_isr);
    idt_set_gate(11, (uintptr_t)i386_exception_11_isr);
    idt_set_gate(12, (uintptr_t)i386_exception_12_isr);
    idt_set_gate(13, (uintptr_t)i386_exception_13_isr);
    idt_set_gate(14, (uintptr_t)i386_exception_14_isr);
    idt_set_gate(15, (uintptr_t)i386_exception_15_isr);
    idt_set_gate(16, (uintptr_t)i386_exception_16_isr);
    idt_set_gate(17, (uintptr_t)i386_exception_17_isr);
    idt_set_gate(18, (uintptr_t)i386_exception_18_isr);
    idt_set_gate(19, (uintptr_t)i386_exception_19_isr);
    idt_set_gate(20, (uintptr_t)i386_exception_20_isr);
    idt_set_gate(21, (uintptr_t)i386_exception_21_isr);
    idt_set_gate(22, (uintptr_t)i386_exception_22_isr);
    idt_set_gate(23, (uintptr_t)i386_exception_23_isr);
    idt_set_gate(24, (uintptr_t)i386_exception_24_isr);
    idt_set_gate(25, (uintptr_t)i386_exception_25_isr);
    idt_set_gate(26, (uintptr_t)i386_exception_26_isr);
    idt_set_gate(27, (uintptr_t)i386_exception_27_isr);
    idt_set_gate(28, (uintptr_t)i386_exception_28_isr);
    idt_set_gate(29, (uintptr_t)i386_exception_29_isr);
    idt_set_gate(30, (uintptr_t)i386_exception_30_isr);
    idt_set_gate(31, (uintptr_t)i386_exception_31_isr);
}

void i386_processor_exceptions_handle(uint8_t exceptionNumber,
                                      const void* gp_regs_ptr,
                                      const void* cpu_frame_ptr,
                                      bool has_error_code)
{

    const char* exceptionName = get_exception_name(exceptionNumber);
    exception_output_ctx_t out = exception_prepare_output();

    // if (!out.use_gfx && !out.use_uart)
    // {
    //     uart_open();
    //     out.use_uart = true;
    // }

#if defined(__x86_64__)
    const uint64_t* regs = (const uint64_t*)gp_regs_ptr;
    const uint64_t* frame = (const uint64_t*)cpu_frame_ptr;

    uint64_t error_code = has_error_code ? frame[0] : 0;
    size_t frame_index = has_error_code ? 1 : 0;

    uint64_t rip = frame[frame_index++];
    uint64_t cs = frame[frame_index++];
    uint64_t rflags = frame[frame_index++];

    bool from_user = (cs & 0x3) != 0;

    uint64_t rsp_at_fault;
    uint64_t ss_value = 0;
    if (from_user)
    {
        rsp_at_fault = frame[frame_index++];
        ss_value = frame[frame_index++];
    }
    else
    {
        rsp_at_fault = (uint64_t)((uintptr_t)frame + frame_index * sizeof(uint64_t));
        ss_value = read_ss();
    }

    struct
    {
        uint64_t rax;
        uint64_t rcx;
        uint64_t rdx;
        uint64_t rbx;
        uint64_t rbp;
        uint64_t rsi;
        uint64_t rdi;
        uint64_t r8;
        uint64_t r9;
        uint64_t r10;
        uint64_t r11;
        uint64_t r12;
        uint64_t r13;
        uint64_t r14;
        uint64_t r15;
    } gpr = {
        .rax = regs[0],
        .rcx = regs[1],
        .rdx = regs[2],
        .rbx = regs[3],
        .rbp = regs[4],
        .rsi = regs[5],
        .rdi = regs[6],
        .r8  = regs[7],
        .r9  = regs[8],
        .r10 = regs[9],
        .r11 = regs[10],
        .r12 = regs[11],
        .r13 = regs[12],
        .r14 = regs[13],
        .r15 = regs[14],
    };

    uint64_t cr0 = read_cr0();
    uint64_t cr2 = read_cr2();
    uint64_t cr3 = read_cr3();
    uint64_t cr4 = read_cr4();
    uint64_t cr8 = read_cr8();

    uint16_t ds = read_ds();
    uint16_t es = read_es();
    uint16_t fs = read_fs();
    uint16_t gs = read_gs();

    exception_printf(&out, "\n==================== CPU EXCEPTION ====================\n");
    exception_printf(&out, "Vector : %u (%s)\n", exceptionNumber, exceptionName);
    exception_printf(&out, "Origin : %s mode\n", from_user ? "user" : "kernel");
    if (has_error_code)
    {
        exception_printf(&out, "Error  : 0x%016llX\n", (unsigned long long)error_code);
    }

    exception_printf(&out, "RIP=%016llX  CS=%04llX  RFLAGS=%016llX\n",
                     (unsigned long long)rip,
                     (unsigned long long)(cs & 0xFFFF),
                     (unsigned long long)rflags);
    exception_printf(&out, "RSP=%016llX  SS=%04llX\n",
                     (unsigned long long)rsp_at_fault,
                     (unsigned long long)(ss_value & 0xFFFF));

    exception_printf(&out, "RAX=%016llX  RBX=%016llX  RCX=%016llX  RDX=%016llX\n",
                     (unsigned long long)gpr.rax,
                     (unsigned long long)gpr.rbx,
                     (unsigned long long)gpr.rcx,
                     (unsigned long long)gpr.rdx);
    exception_printf(&out, "RSI=%016llX  RDI=%016llX  RBP=%016llX\n",
                     (unsigned long long)gpr.rsi,
                     (unsigned long long)gpr.rdi,
                     (unsigned long long)gpr.rbp);
    exception_printf(&out, " R8=%016llX   R9=%016llX  R10=%016llX  R11=%016llX\n",
                     (unsigned long long)gpr.r8,
                     (unsigned long long)gpr.r9,
                     (unsigned long long)gpr.r10,
                     (unsigned long long)gpr.r11);
    exception_printf(&out, "R12=%016llX  R13=%016llX  R14=%016llX  R15=%016llX\n",
                     (unsigned long long)gpr.r12,
                     (unsigned long long)gpr.r13,
                     (unsigned long long)gpr.r14,
                     (unsigned long long)gpr.r15);

    exception_printf(&out, "CR0=%016llX  CR2=%016llX  CR3=%016llX  CR4=%016llX  CR8=%016llX\n",
                     (unsigned long long)cr0,
                     (unsigned long long)cr2,
                     (unsigned long long)cr3,
                     (unsigned long long)cr4,
                     (unsigned long long)cr8);

    exception_printf(&out, "DS=%04llX  ES=%04llX  FS=%04llX  GS=%04llX\n",
                     (unsigned long long)(ds & 0xFFFF),
                     (unsigned long long)(es & 0xFFFF),
                     (unsigned long long)(fs & 0xFFFF),
                     (unsigned long long)(gs & 0xFFFF));

#else
    const uint32_t* regs = (const uint32_t*)gp_regs_ptr;
    const uint32_t* frame = (const uint32_t*)cpu_frame_ptr;

    uint32_t error_code = has_error_code ? frame[0] : 0;
    size_t frame_index = has_error_code ? 1 : 0;

    uint32_t eip = frame[frame_index++];
    uint32_t cs = frame[frame_index++];
    uint32_t eflags = frame[frame_index++];

    bool from_user = (cs & 0x3) != 0;

    uint32_t esp_at_fault;
    uint32_t ss_value = 0;

    if (from_user)
    {
        esp_at_fault = frame[frame_index++];
        ss_value = frame[frame_index++];
    }
    else
    {
        esp_at_fault = (uint32_t)((uintptr_t)frame + frame_index * sizeof(uint32_t));
        ss_value = read_ss();
    }

    struct
    {
        uint32_t eax;
        uint32_t ebx;
        uint32_t ecx;
        uint32_t edx;
        uint32_t esi;
        uint32_t edi;
        uint32_t ebp;
        uint32_t esp;
    } gpr = {
        .eax = regs[7],
        .ecx = regs[6],
        .edx = regs[5],
        .ebx = regs[4],
        .esp = regs[3],
        .ebp = regs[2],
        .esi = regs[1],
        .edi = regs[0],
    };

    uint32_t cr0 = read_cr0();
    uint32_t cr2 = read_cr2();
    uint32_t cr3 = read_cr3();
    uint32_t cr4 = read_cr4();

    uint16_t ds = read_ds();
    uint16_t es = read_es();
    uint16_t fs = read_fs();
    uint16_t gs = read_gs();

    exception_printf(&out, "\n==================== CPU EXCEPTION ====================\n");
    exception_printf(&out, "Vector : %u (%s)\n", exceptionNumber, exceptionName);
    exception_printf(&out, "Origin : %s mode\n", from_user ? "user" : "kernel");
    if (has_error_code)
    {
        exception_printf(&out, "Error  : 0x%08X\n", error_code);
    }

    exception_printf(&out, "EIP=%08X  CS=%04X  EFLAGS=%08X\n",
                     eip,
                     cs & 0xFFFF,
                     eflags);
    exception_printf(&out, "ESP=%08X  SS=%04X\n",
                     esp_at_fault,
                     ss_value & 0xFFFF);

    exception_printf(&out, "EAX=%08X  EBX=%08X  ECX=%08X  EDX=%08X\n",
                     gpr.eax,
                     gpr.ebx,
                     gpr.ecx,
                     gpr.edx);
    exception_printf(&out, "ESI=%08X  EDI=%08X  EBP=%08X  ESP(snap)=%08X\n",
                     gpr.esi,
                     gpr.edi,
                     gpr.ebp,
                     gpr.esp);

    exception_printf(&out, "CR0=%08X  CR2=%08X  CR3=%08X  CR4=%08X\n",
                     cr0,
                     cr2,
                     cr3,
                     cr4);

    exception_printf(&out, "DS=%04X  ES=%04X  FS=%04X  GS=%04X\n",
                     ds & 0xFFFF,
                     es & 0xFFFF,
                     fs & 0xFFFF,
                     gs & 0xFFFF);
#endif

    if (out.use_gfx)
    {
        exception_printf(&out, "Output : GFX debug terminal\n");
    }
    else
    {
        exception_printf(&out, "Output : UART (GFX terminal unavailable)\n");
    }

    exception_printf(&out, "=======================================================\n");

    if (out.use_gfx)
    {
        debugterm_flush();
    }

    WARN("CPU exception: vector=%u (%s)", exceptionNumber, exceptionName);

    acpi_restart();

    ASSERT(isError(exceptionNumber) == false, "CPU EXCEPTION");
}

