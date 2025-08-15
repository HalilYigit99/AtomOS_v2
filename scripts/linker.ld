/* AtomOS Kernel Linker Script
 * Compatible with both 32-bit and 64-bit architectures
 * Load address: 1MB (0x100000) - Classic kernel location
 */

ENTRY(_start)

SECTIONS
{
    /* Kernel starts at 1MB physical address */
    . = 0x100000;
    
    /* Multiboot header must be early in the file */
    .multiboot : ALIGN(4K)
    {
        *(.multiboot)
    }
    
    /* Text section - executable code */
    .text : ALIGN(4K)
    {
        *(.text)
        *(.text.*)
    }
    
    /* Read-only data */
    .rodata : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }
    
    /* Initialized data */
    .data : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }
    
    /* Uninitialized data (BSS) */
    .bss : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
    
    /* Stack section (optional, for debugging) */
    .stack : ALIGN(4K)
    {
        __stack_start = .;
        . = . + 0x4000; /* 16KB stack */
        __stack_end = .;
    }
    
    /* Define symbols for kernel sections */
    __kernel_start = 0x100000;
    __kernel_end = .;
    __kernel_size = __kernel_end - __kernel_start;
    
    /* Text section boundaries */
    __text_start = ADDR(.text);
    __text_end = ADDR(.text) + SIZEOF(.text);
    
    /* Data section boundaries */
    __data_start = ADDR(.data);
    __data_end = ADDR(.data) + SIZEOF(.data);
    
    /* BSS section boundaries */
    __bss_start = ADDR(.bss);
    __bss_end = ADDR(.bss) + SIZEOF(.bss);
    
    /* Discard unnecessary sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note)
        *(.note.*)
        /* Debug/backtrace için .eh_frame ve .eh_frame_hdr TUTULUYOR */
    }
        /* Debug/backtrace için .eh_frame ve .eh_frame_hdr TUTULUYOR */
        /* .eh_frame ve .eh_frame_hdr bölümleri gereksiz olarak atılmaktadır */
}