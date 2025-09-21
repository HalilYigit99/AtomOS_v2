#include <arch.h>
#include <memory/memory.h>
#include <debug/debug.h>

extern void bios_int();

extern uint8_t  bios_int_no;

extern uint16_t bios_ax;
extern uint16_t bios_bx;
extern uint16_t bios_cx;
extern uint16_t bios_dx;
extern uint16_t bios_si;
extern uint16_t bios_di;
extern uint16_t bios_bp;
extern uint16_t bios_sp;
extern uint16_t bios_es;
extern uint16_t bios_ds;
extern uint16_t bios_fs;
extern uint16_t bios_gs;
extern uint16_t bios_ss;
extern uint16_t bios_flags;
extern uint16_t bios_cs;

// Provided from linker script
extern char __bios_code_start;
extern char __bios_code_end;

static bool bios_code_copied = false;

void i386_bios_int(uint8_t int_no, arch_processor_regs_t* in, arch_processor_regs_t* out)
{

    arch_processor_regs_t defaults = {0};

    if (!in) {
        in = &defaults;
    }

    if (!bios_code_copied) {
        memcpy(&__bios_code_start, &__bios_code_start, &__bios_code_end - &__bios_code_start);
        bios_code_copied = true;
    }

    bios_int_no = int_no;

    bios_ax = in->ax;
    bios_bx = in->bx;
    bios_cx = in->cx;
    bios_dx = in->dx;
    bios_si = in->si;
    bios_di = in->di;
    bios_bp = in->bp;
    bios_sp = in->sp;
    bios_ds = in->ds;
    bios_es = in->es;
    bios_fs = in->fs;
    bios_gs = in->gs;
    bios_ss = in->ss;
    bios_flags = in->flags;
    bios_cs = in->cs;

    bios_int();

    if (!out) {
        return;
    }

    out->ax = bios_ax;
    out->bx = bios_bx;
    out->cx = bios_cx;
    out->dx = bios_dx;
    out->si = bios_si;
    out->di = bios_di;
    out->bp = bios_bp;
    out->sp = bios_sp;
    out->ds = bios_ds;
    out->es = bios_es;
    out->fs = bios_fs;
    out->gs = bios_gs;
    out->ss = bios_ss;
    out->flags = bios_flags;
    out->cs = bios_cs;

}
