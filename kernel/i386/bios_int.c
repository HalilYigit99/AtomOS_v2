#include <arch.h>
#include <memory/memory.h>
#include <debug/debug.h>

extern void bios_int();

extern uint16_t bios_ax;
extern uint16_t bios_bx;
extern uint16_t bios_cx;
extern uint16_t bios_dx;

// Provided from linker script
extern char __bios_code_start;
extern char __bios_code_end;

void i386_bios_int(uint8_t int_no, arch_processor_regs_t* in, arch_processor_regs_t* out)
{
    LOG("Not supported yet");
}
