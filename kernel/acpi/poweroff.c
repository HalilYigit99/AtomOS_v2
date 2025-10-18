#include <acpi/acpi.h>
#include <acpi/acpi_new.h>
#include <acpi/acpi_old.h>
#include <acpi/fadt.h>
#include <boot/multiboot2.h>
#include <efi/efi.h>
#include <debug/debug.h>
#include <arch.h>

// ACPI PM1 Control register bit fields (ACPI 1.0 semantics)
#define ACPI_PM1_CNT_SCI_EN   (1u << 0)
#define ACPI_PM1_CNT_SLP_EN   (1u << 13)
#define ACPI_PM1_CNT_SLP_TYP_SHIFT 10

// Minimal AML helpers for parsing _S5_ Package integers
static bool aml_parse_pkg_length(const uint8_t* p, size_t* consumed, size_t* length)
{
    if (!p || !consumed || !length) return false;
    uint8_t b0 = p[0];
    uint8_t bytes_follow = (b0 >> 6) & 0x3; // 0..3
    if (bytes_follow == 0) {
        *consumed = 1;
        *length = b0 & 0x3F;
        return true;
    }
    uint32_t len = b0 & 0x0F; // low 4 bits are least significant byte
    for (uint8_t i = 0; i < bytes_follow; i++) {
        len |= ((uint32_t)p[1 + i]) << (8 * (i + 1));
    }
    *consumed = 1 + bytes_follow;
    *length = len;
    return true;
}

static bool aml_parse_integer(const uint8_t* p, size_t max_len, uint64_t* value_out, size_t* consumed)
{
    if (!p || !value_out || !consumed || max_len == 0) return false;
    uint8_t op = p[0];
    switch (op) {
        case 0x00: // ZeroOp
            *value_out = 0; *consumed = 1; return true;
        case 0x01: // OneOp
            *value_out = 1; *consumed = 1; return true;
        case 0x0A: // ByteConst
            if (max_len < 2) return false;
            *value_out = p[1]; *consumed = 2; return true;
        case 0x0B: // WordConst
            if (max_len < 3) return false;
            *value_out = (uint64_t)(p[1] | (p[2] << 8)); *consumed = 3; return true;
        case 0x0C: // DWordConst
            if (max_len < 5) return false;
            *value_out = (uint64_t)(p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24)); *consumed = 5; return true;
        case 0x0E: // QWordConst
            if (max_len < 9) return false;
            {
                uint64_t v = 0;
                for (int i = 0; i < 8; i++) v |= ((uint64_t)p[1 + i]) << (8 * i);
                *value_out = v; *consumed = 9; return true;
            }
        default:
            return false;
    }
}

static bool acpi_find_s5_slp_typ(const acpi_sdt_header* dsdt, uint8_t* slp_typ_a, uint8_t* slp_typ_b)
{
    if (!dsdt || !slp_typ_a || !slp_typ_b) return false;
    const uint8_t* aml = (const uint8_t*)(dsdt + 1);
    size_t aml_len = dsdt->Length - sizeof(acpi_sdt_header);

    // Search for the name string "_S5_" in AML stream
    for (size_t i = 0; i + 4 < aml_len; i++) {
        if (aml[i] == '_' && aml[i+1] == 'S' && aml[i+2] == '5' && aml[i+3] == '_') {
            // Expect NameOp (0x08) just before the name (common case)
            size_t name_pos = i;
            if (name_pos == 0) continue;
            if (aml[name_pos - 1] != 0x08) continue; // not a NameOp

            // After name, typically a PackageOp (0x12)
            size_t p = name_pos + 4; // point to opcode after name
            if (p >= aml_len) continue;
            if (aml[p] != 0x12) continue; // not a Package
            p++;

            // Parse PkgLength to skip header
            size_t pkg_len_field = 0, pkg_body_len = 0;
            if (!aml_parse_pkg_length(&aml[p], &pkg_len_field, &pkg_body_len)) continue;
            p += pkg_len_field;
            if (p >= aml_len) continue;

            // NumElements
            if (p >= aml_len) continue;
            uint8_t elem_count = aml[p++];
            if (elem_count < 2) continue;

            // First element: SLP_TYPa integer
            uint64_t v1 = 0, v2 = 0; size_t c1 = 0, c2 = 0;
            if (!aml_parse_integer(&aml[p], aml_len - p, &v1, &c1)) continue;
            p += c1;
            if (!aml_parse_integer(&aml[p], aml_len - p, &v2, &c2)) continue;

            *slp_typ_a = (uint8_t)(v1 & 0x7F);
            *slp_typ_b = (uint8_t)(v2 & 0x7F);
            return true;
        }
    }
    return false;
}

static void acpi_enable_legacy_if_needed(const acpi_fadt_unified* fadt)
{
    if (!fadt) return;
    // If SCI_EN not set and we have SMI_CMD + AcpiEnable, try enabling ACPI
    if (fadt->Pm1aCntBlk == 0) return;
    uint16_t pm1a = (uint16_t)fadt->Pm1aCntBlk;
    uint16_t pm1b = (uint16_t)fadt->Pm1bCntBlk;
    uint16_t v = inw(pm1a);
    if ((v & ACPI_PM1_CNT_SCI_EN) == 0 && fadt->SmiCmd != 0 && fadt->AcpiEnable != 0) {
        LOG("ACPI: Enabling ACPI via SMI_CMD=0x%X, value=0x%X", (unsigned)fadt->SmiCmd, fadt->AcpiEnable);
        outb((uint16_t)fadt->SmiCmd, fadt->AcpiEnable);
        // Poll for SCI_EN set
        for (int i = 0; i < 100000; i++) {
            io_wait();
            v = inw(pm1a);
            if (v & ACPI_PM1_CNT_SCI_EN) break;
        }
        // Optionally read PM1B too
        if (pm1b) {
            (void)inw(pm1b);
        }
    }
}

static const acpi_sdt_header* acpi_get_dsdt_from_fadt(const acpi_fadt_unified* fadt)
{
    if (!fadt) return NULL;
    // Prefer XDsdt if present (v2+), else Dsdt
    if (fadt->XDsdt) {
        return (const acpi_sdt_header*)(uintptr_t)fadt->XDsdt;
    }
    if (fadt->Dsdt) {
        return (const acpi_sdt_header*)(uintptr_t)fadt->Dsdt;
    }
    return NULL;
}

static void acpi_get_pm1_ports(const acpi_fadt_unified* fadt, uint16_t* pm1a, uint16_t* pm1b)
{
    // Default to legacy fields
    uint16_t a = (uint16_t)fadt->Pm1aCntBlk;
    uint16_t b = (uint16_t)fadt->Pm1bCntBlk;
    // Prefer XPm1* if address space is System I/O (1) and address non-zero
    if (fadt->XPm1aCntBlk.Address && fadt->XPm1aCntBlk.AddressSpaceId == 1) {
        a = (uint16_t)fadt->XPm1aCntBlk.Address;
    }
    if (fadt->XPm1bCntBlk.Address && fadt->XPm1bCntBlk.AddressSpaceId == 1) {
        b = (uint16_t)fadt->XPm1bCntBlk.Address;
    }
    if (pm1a) *pm1a = a;
    if (pm1b) *pm1b = b;
}

static void acpi_enter_s5_via_ports(uint16_t pm1a, uint16_t pm1b, uint8_t slp_typ_a, uint8_t slp_typ_b)
{
    // Read-Modify-Write to preserve other control bits (e.g., SCI_EN)
    uint16_t val = inw(pm1a);
    val &= (uint16_t)~(0x7u << ACPI_PM1_CNT_SLP_TYP_SHIFT);
    val |= (uint16_t)((slp_typ_a & 0x7) << ACPI_PM1_CNT_SLP_TYP_SHIFT);
    val |= ACPI_PM1_CNT_SLP_EN;
    LOG("ACPI: Entering S5 via PM1a=0x%X, SLP_TYPa=0x%X", pm1a, slp_typ_a);
    outw(pm1a, val);

    if (pm1b) {
        uint16_t valb = inw(pm1b);
        valb &= (uint16_t)~(0x7u << ACPI_PM1_CNT_SLP_TYP_SHIFT);
        valb |= (uint16_t)((slp_typ_b & 0x7) << ACPI_PM1_CNT_SLP_TYP_SHIFT);
        valb |= ACPI_PM1_CNT_SLP_EN;
        LOG("ACPI: Entering S5 via PM1b=0x%X, SLP_TYPb=0x%X", pm1b, slp_typ_b);
        outw(pm1b, valb);
    }
}

void acpi_poweroff() {

    LOG("Shutdown process started!");

    // // Is EFI boot?
    // if (mb2_is_efi_boot) {
    //     EFI_SYSTEM_TABLE* systab = efi_system_table;
    //     systab->runtime_services->reset_system(EfiResetShutdown, 0, 0, NULL);
    // }

    // Check if the ACPI is available
    
    if (acpi_version < 2)
    {
        // ACPI 1.0 için S5 (soft-off) uygula: _S5_ paketinden SLP_TYP al, PM1a/b CNT'ye yaz
        const acpi_fadt_unified* fadt = (const acpi_fadt_unified*)acpi_get_fadt();
        if (!fadt) {
            ERROR("ACPI: FADT not found");
            return;
        }

        if (fadt->Pm1aCntBlk == 0) {
            ERROR("ACPI: PM1aCntBlk is zero; cannot enter S5");
            return;
        }

        // ACPI'yi gerekirse etkinleştir
        acpi_enable_legacy_if_needed(fadt);

        // DSDT'den _S5_ değerlerini bul
        const acpi_sdt_header* dsdt = acpi_get_dsdt_from_fadt(fadt);
        if (!dsdt || dsdt->Length < sizeof(acpi_sdt_header)) {
            ERROR("ACPI: DSDT not available or invalid");
            return;
        }

        uint8_t slp_typ_a = 0, slp_typ_b = 0;
        if (!acpi_find_s5_slp_typ(dsdt, &slp_typ_a, &slp_typ_b)) {
            WARN("ACPI: _S5_ not found in DSDT; falling back to SLP_TYP=5");
            slp_typ_a = 5; slp_typ_b = 5;
        }

        uint16_t pm1a = 0, pm1b = 0;
        acpi_get_pm1_ports(fadt, &pm1a, &pm1b);
        acpi_enter_s5_via_ports(pm1a, pm1b, slp_typ_a, slp_typ_b);

    }else 
    {
        // ACPI 2.0+ için S5 (soft-off) uygula: XDsdt/XPm1* ve _S5_
        const acpi_fadt_unified* fadt = (const acpi_fadt_unified*)acpi_get_fadt();
        if (!fadt) {
            ERROR("ACPI: FADT not found");
            return;
        }

        // ACPI'yi gerekirse etkinleştir (legacy alan üzerinden poll yeterli)
        acpi_enable_legacy_if_needed(fadt);

        const acpi_sdt_header* dsdt = acpi_get_dsdt_from_fadt(fadt);
        if (!dsdt || dsdt->Length < sizeof(acpi_sdt_header)) {
            ERROR("ACPI: DSDT not available or invalid");
            return;
        }

        uint8_t slp_typ_a = 0, slp_typ_b = 0;
        if (!acpi_find_s5_slp_typ(dsdt, &slp_typ_a, &slp_typ_b)) {
            WARN("ACPI: _S5_ not found in DSDT; falling back to SLP_TYP=5");
            slp_typ_a = 5; slp_typ_b = 5;
        }

        uint16_t pm1a = 0, pm1b = 0;
        acpi_get_pm1_ports(fadt, &pm1a, &pm1b);
        if (pm1a == 0) {
            ERROR("ACPI: PM1aCntBlk not available; cannot enter S5");
            return;
        }

        acpi_enter_s5_via_ports(pm1a, pm1b, slp_typ_a, slp_typ_b);

    }

    // Busy wait loop; sistem kapanmalı
    for (int i = 0; i < 1000000; ++i) io_wait();

    ERROR("System did not power off; Trying keyboard controller method");

    // 8042 klavye denetleyici reseti: komut 0xFE
    // Giriş tamponu boşalana kadar bekle (bit1 = IBF)
    for (int i = 0; i < 1000000; ++i)
    {
        if ((inb(0x64) & 0x02) == 0) break;
        io_wait();
    }
    outb(0x64, 0xFE);

    // Eğer hiçbir yöntem çalışmazsa CPU'yu durdur
    ERROR("System did not power off; halting");
    while (1) {
        asm volatile ("hlt");
        asm volatile ("pause");
        asm volatile ("nop");
    }

}

void acpi_restart()
{
    // Önce ACPI 2.0+ FADT ResetReg varsa onu kullan, sonra chipset reset portu (0xCF9),
    // en sonda 8042 klavye denetleyicisi ile reset dene.

    // if (mb2_is_efi_boot)
    // {
    //     EFI_SYSTEM_TABLE* systab = efi_system_table;
    //     systab->runtime_services->reset_system(EfiResetWarm, 0,0, NULL);
    // }

    const acpi_fadt_unified* fadt = (const acpi_fadt_unified*)acpi_get_fadt();
    if (fadt) {
        const acpi_gas* rr = &fadt->ResetReg;
        if (rr->Address != 0 && fadt->ResetValue != 0) {
            LOG("ACPI: Trying ResetReg (ASID=%u, Width=%u, AccessSize=%u, Addr=0x%llx, Val=0x%02x)",
                rr->AddressSpaceId, rr->RegisterBitWidth, rr->AccessSize,
                (unsigned long long)rr->Address, fadt->ResetValue);

            uint8_t val = fadt->ResetValue;

            // Erişim boyutunu belirle (AccessSize > 0 ise onu kullan, yoksa bit genişliğinden türet)
            uint8_t access_size = rr->AccessSize;
            if (access_size == 0) {
                if (rr->RegisterBitWidth <= 8) access_size = 1;
                else if (rr->RegisterBitWidth <= 16) access_size = 2;
                else if (rr->RegisterBitWidth <= 32) access_size = 3;
                else access_size = 4; // 64-bit
            }

            switch (rr->AddressSpaceId) {
                case 1: { // System I/O
                    uint16_t port = (uint16_t)rr->Address;
                    // Pek çok platform 8-bit yazımı bekler; daha genişlikler için en azından uyumlu yazalım
                    if (access_size == 1) {
                        outb(port, val);
                    } else if (access_size == 2) {
                        outw(port, (uint16_t)val);
                    } else {
                        outl(port, (uint32_t)val);
                    }
                    // Başarılıysa sistem resetlenmeli; kısa bir bekleme
                    for (int i = 0; i < 1000000; ++i) io_wait();
                    break;
                }
                case 0: { // System Memory (MMIO)
                    volatile void* p = (volatile void*)(uintptr_t)rr->Address;
                    if (access_size == 1) {
                        *(volatile uint8_t*)p = val;
                    } else if (access_size == 2) {
                        *(volatile uint16_t*)p = (uint16_t)val;
                    } else if (access_size == 3) {
                        *(volatile uint32_t*)p = (uint32_t)val;
                    } else {
                        *(volatile uint64_t*)p = (uint64_t)val;
                    }
                    for (int i = 0; i < 1000000; ++i) io_wait();
                    break;
                }
                default:
                    WARN("ACPI: Unsupported ResetReg AddressSpaceId=%u", rr->AddressSpaceId);
                    break;
            }
        }
    }

    // Yonga seti reset portu: 0xCF9 (Reset Control).
    // Yaygın sıralama: önce 0x02 (CPU reset bit), ardından 0x06 (SYS_RST | RST_CPU).
    LOG("ACPI: Falling back to chipset reset via 0xCF9");
    outb(0xCF9, 0x02);
    io_wait();
    outb(0xCF9, 0x06);
    for (int i = 0; i < 1000000; ++i) io_wait();

    // 8042 klavye denetleyici reseti: komut 0xFE
    LOG("ACPI: Falling back to 8042 keyboard controller reset");
    // Giriş tamponu boşalana kadar bekle (bit1 = IBF)
    for (int i = 0; i < 1000000; ++i) {
        if ((inb(0x64) & 0x02) == 0) break;
        io_wait();
    }
    outb(0x64, 0xFE);

    // Eğer hiçbir yöntem çalışmazsa CPU'yu durdur
    ERROR("System restart did not occur; halting");
    while (1) asm volatile ("hlt");
}
