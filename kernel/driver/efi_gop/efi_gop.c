#include <driver/DriverBase.h>
#include <boot/multiboot2.h>
#include <efi/efi.h>
#include <debug/debug.h>
#include <graphics/screen.h>
#include <memory/memory.h>
#include <list.h>
#include <driver/efi_gop/efi_gop.h>

extern DriverBase efi_gop_driver;


static size_t count_bits(uint32_t x)
{
    size_t c = 0;
    while (x) { c += (x & 1u); x >>= 1u; }
    return c;
}

static size_t gop_info_bpp(const EFI_GRAPHICS_OUTPUT_MODE_INFO* info)
{
    if (!info) return 0;
    switch (info->pixel_format)
    {
        case PixelRedGreenBlueReserved8BitPerColor:
        case PixelBlueGreenRedReserved8BitPerColor:
            return 32; // 8:8:8:8
        case PixelBitMask: {
            size_t r = count_bits(info->red_mask);
            size_t g = count_bits(info->green_mask);
            size_t b = count_bits(info->blue_mask);
            size_t a = count_bits(info->reserved_mask);
            size_t total = r + g + b + a;
            // Bazı firmware'ler 24bpp raporlayabilir (a=0), çoğu 32bpp kullanır
            if (total == 0) total = 32; // güvenli varsayım
            return total;
        }
        case PixelBltOnly:
        default:
            return 0; // framebuffer'a erişim yok veya bilinmeyen format
    }
}

bool efi_gop_detect(ScreenInfo* screen)
{
    if (!screen) return false;

    if (!mb2_is_efi_boot) {
        WARN("efi_gop: EFI modunda değiliz, GOP enumerate atlandı");
        return false;
    }

    ASSERT(efi_system_table, "EFI System Table is NULL");
    ASSERT(efi_system_table->boot_services, "EFI Boot Services is NULL");

    EFI_GUID gop_guid = (EFI_GUID)EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EFI_STATUS st = efi_system_table->boot_services->locate_protocol(&gop_guid, NULL, (void**)&gop);
    if (IS_EFI_ERROR(st) || !gop || !gop->mode) {
        ERROR("efi_gop: GOP locate_protocol başarısız (st=%lu, gop=%p)", st, gop);
        return false;
    }

    uint32_t current_mode = gop->mode->mode;
    uint32_t max_mode = gop->mode->max_mode;
    LOG("efi_gop: max_mode=%u, current_mode=%u", max_mode, current_mode);

    if (!screen->video_modes)
        screen->video_modes = List_Create();
    else if (screen->video_modes->count)
        List_Clear(screen->video_modes, true);

    for (uint32_t i = 0; i < max_mode; ++i)
    {
        EFI_GRAPHICS_OUTPUT_MODE_INFO* info = NULL;
        UINTN info_size = 0;
        st = gop->query_mode(gop, i, &info_size, &info);
        if (IS_EFI_ERROR(st) || !info) {
            WARN("efi_gop: query_mode(%u) başarısız (st=%lu)", i, st);
            continue;
        }

        size_t bpp = gop_info_bpp(info);
        if (bpp == 0) {
            WARN("efi_gop: mode %u BLT-only veya desteklenmiyor, atlandı", i);
            continue;
        }

        size_t bytes_per_pixel = bpp / 8;
        size_t pitch_bytes = (size_t)info->pixels_per_scan_line * bytes_per_pixel;

        ScreenVideoModeInfo* mode = (ScreenVideoModeInfo*)malloc(sizeof(ScreenVideoModeInfo));
        if (!mode) {
            ERROR("efi_gop: mode nesnesi için bellek ayrılamadı");
            break;
        }
        mode->mode_number = i;
        mode->width = info->horizontal_resolution;
        mode->height = info->vertical_resolution;
        mode->bpp = bpp;
        mode->pitch = pitch_bytes;
        mode->framebuffer = NULL; // aktif moda geçince dolduracağız
        mode->linear_framebuffer = true;

        List_Add(screen->video_modes, mode);
    }

    return true;
}

bool efi_gop_init()
{
    if (efi_gop_driver.enabled) {
        LOG("efi_gop: zaten etkin, init atlandı");
        return true;
    }

    if (!mb2_is_efi_boot) {
        WARN("efi_gop: EFI modunda değiliz, init atlandı");
        return false;
    }

    efi_gop_detect(&main_screen);

    return true;

}

void efi_gop_enable()
{
    if (!efi_gop_driver.enabled) {
        efi_gop_driver.enabled = true;
        LOG("efi_gop: etkinleştirildi");
    }
}

void efi_gop_disable()
{
    if (efi_gop_driver.enabled) {
        efi_gop_driver.enabled = false;
        LOG("efi_gop: devre dışı bırakıldı");
    }
}

void efi_gop_setVideoMode(ScreenInfo* screen, ScreenVideoModeInfo* mode)
{
    if (!screen || !mode) {
        ERROR("efi_gop_setVideoMode: Geçersiz parametreler");
        return;
    }

    if (!mb2_is_efi_boot) {
        WARN("efi_gop_setVideoMode: EFI modunda değiliz, moda geçiş atlandı");
        return;
    }

    ASSERT(efi_system_table, "EFI System Table is NULL");
    ASSERT(efi_system_table->boot_services, "EFI Boot Services is NULL");

    EFI_GUID gop_guid = (EFI_GUID)EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = NULL;
    EFI_STATUS st = efi_system_table->boot_services->locate_protocol(&gop_guid, NULL, (void**)&gop);
    if (IS_EFI_ERROR(st) || !gop || !gop->mode) {
        ERROR("efi_gop_setVideoMode: GOP locate_protocol başarısız (st=%lu, gop=%p)", st, gop);
        return;
    }

    st = gop->set_mode(gop, mode->mode_number);
    if (IS_EFI_ERROR(st)) {
        ERROR("efi_gop_setVideoMode: set_mode(%u) başarısız (st=%lu)", mode->mode_number, st);
        return;
    }

    // Framebuffer taban adresini güncelle
    if (gop->mode && gop->mode->info && gop->mode->frame_buffer_base) {
        mode->framebuffer = (void*)(uintptr_t)(gop->mode->frame_buffer_base);
        screen->mode = mode;
        LOG("efi_gop_setVideoMode: Mod %ux%u, %zubpp (%u) etkinleştirildi, fb=%p",
            mode->width, mode->height, mode->bpp, mode->mode_number, mode->framebuffer);
    } else {
        WARN("efi_gop_setVideoMode: Mod etkinleştirildi ancak framebuffer bilgisi alınamadı");
    }
}

DriverBase efi_gop_driver = {
    .name = "EFI Graphics Output Protocol Driver",
    .enabled = false,
    .version = 1,
    .context = NULL,
    .init = efi_gop_init,    // (opsiyonel) sürücü katmanı ile entegrasyon
    .enable = efi_gop_enable,  // Enable function can be assigned here
    .disable = efi_gop_disable, // Disable function can be assigned here
    .type = DRIVER_TYPE_DISPLAY
};
