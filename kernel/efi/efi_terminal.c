#include <efi/efi.h>
#include <memory/memory.h>
#include <util/VPrintf.h>
#include <debug/debug.h>
#include <stream/OutputStream.h>

char* efiterm_content;
size_t efiterm_content_size;
size_t efiterm_content_capacity;

static void efiterm_init()
{

    efiterm_content = malloc(16 * 1024);

    if (!efiterm_content) 
    {
        LOG("Efi terminal content buffer allocation failed!");
        return;
    }

    efiterm_content_capacity = 16 * 1024;
    efiterm_content_size = 0;



}

static void __attribute__((unused)) efiterm_putc(char c)
{

}


OutputStream efi_stdout =
{
    .Open = efiterm_init,
};

DebugStream efi_debugStream;
