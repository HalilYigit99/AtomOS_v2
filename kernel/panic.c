#include <panic.h>
#include <debug/debug.h>


void PANIC(const char* msg) {
    // In a real kernel, you would likely want to log this message to a serial console or display it on the screen.
    // For simplicity, we'll just enter an infinite loop here.

    asm volatile("cli"); // Disable interrupts

    LOG("KERNEL PANIC: %s\n", msg);
    
    while (1) {
        // Optionally, you could add code to blink an LED or halt the CPU.
        asm volatile("hlt" ::: "memory");
    }
}

