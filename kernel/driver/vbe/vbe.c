#include <driver/DriverBase.h>
#include <graphics/types.h>
#include <boot/multiboot2.h>

static bool vbe_init()
{
    

    return true;
}

DriverBase __attribute__((unused)) vbe_driver = {
    .name = "VBE Driver",
    .version = 1,
    .context = NULL,
    .enabled = false,
    .init = vbe_init,    // VBE init function
    .enable = NULL,  // VBE enable function
    .disable = NULL, // VBE disable function
    .type = DRIVER_TYPE_ANY
};
