/*

    This file provides intel i386's first 32 idt entry exception handling.
    This file is arch independed

*/

#include <arch.h>
#include <debug/debug.h>

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

static bool isError(size_t exceptionId)
{
    size_t __attribute__((unused)) a = exceptionId;
    return true;

    // // True  => Unrecoverable, system should shut down
    // // False => Potentially recoverable/intentional, system may continue
    // switch (exceptionId)
    // {
    // // Recoverable/intentional traps/faults
    // case 1:  // Debug
    // case 2:  // Non-Maskable Interrupt
    // case 3:  // Breakpoint
    // case 4:  // Overflow
    // case 5:  // BOUND Range Exceeded
    // case 7:  // Device Not Available (used for lazy FPU)
    // case 14: // Page Fault (can be handled by pager)
    // case 16: // x87 FPU Floating-Point Error
    // case 17: // Alignment Check
    // case 19: // SIMD Floating-Point Exception
    // case 20: // Virtualization Exception
    //     return true;

    // // Everything else is considered fatal by default
    // default:
    //     return false;
    // }
}

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

void i386_processor_exceptions_handle(uint8_t exceptionNumber)
{

    char* exceptionName = "(null)";

    switch (exceptionNumber)
    {
    case 0:
        exceptionName = "Divide Error";
        break;

    case 1:
        exceptionName = "Debug";
        break;

    case 2:
        exceptionName = "Non-Maskable Interrupt";
        break;

    case 3:
        exceptionName = "Breakpoint";
        break;

    case 4:
        exceptionName = "Overflow";
        break;

    case 5:
        exceptionName = "BOUND Range Exceeded";
        break;

    case 6:
        exceptionName = "Invalid Opcode";
        break;

    case 7:
        exceptionName = "Device Not Available";
        break;

    case 8:
        exceptionName = "Double Fault";
        break;

    case 9:
        exceptionName = "Coprocessor Segment Overrun";
        break;

    case 10:
        exceptionName = "Invalid TSS";
        break;

    case 11:
        exceptionName = "Segment Not Present";
        break;

    case 12:
        exceptionName = "Stack-Segment Fault";
        break;

    case 13:
        exceptionName = "General Protection Fault";
        break;

    case 14:
        exceptionName = "Page Fault";
        break;

    case 15:
        exceptionName = "Reserved";
        break;

    case 16:
        exceptionName = "x87 FPU Floating-Point Error";
        break;

    case 17:
        exceptionName = "Alignment Check";
        break;

    case 18:
        exceptionName = "Machine Check";
        break;

    case 19:
        exceptionName = "SIMD Floating-Point Exception";
        break;

    case 20:
        exceptionName = "Virtualization Exception";
        break;

    case 21:
        exceptionName = "Control Protection Exception";
        break;

    case 22:
        exceptionName = "Reserved";
        break;

    case 23:
        exceptionName = "Reserved";
        break;

    case 24:
        exceptionName = "Reserved";
        break;

    case 25:
        exceptionName = "Reserved";
        break;

    case 26:
        exceptionName = "Reserved";
        break;

    case 27:
        exceptionName = "Reserved";
        break;

    case 28:
        exceptionName = "Reserved";
        break;

    case 29:
        exceptionName = "Reserved";
        break;

    case 30:
        exceptionName = "Security Exception";
        break;

    case 31:
        exceptionName = "Reserved";
        break;
    
    default:
        ERROR("uint8_t exceptionNumber: %d", (size_t)exceptionNumber);
        ASSERT(exceptionNumber < 32, "ANORMAL 'exceptionNumber' arrived!");
        break;
    }

    WARN("CPU exception arrived: '%s'", exceptionName);
    ASSERT(isError(exceptionNumber) == false, "CPU EXCEPTION!!");

}
