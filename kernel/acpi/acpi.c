#include <acpi/acpi.h>
#include <acpi/acpi_old.h>
#include <acpi/acpi_new.h>

#include <boot/multiboot2.h>
#include <debug/debug.h>
#include <util/string.h>
#include <stdint.h>
#include <stddef.h>

/*
 * Basit ACPI init:
 * - Multiboot2 ACPI tag'lerinden RSDP'yi al
 * - Checksum doğrulaması yap
 * - XSDT (>=v2) varsa onu, yoksa RSDT'yi tara
 * - İlgi çekici tabloları (MADT/FADT/HPET/MCFG) bul ve adreslerini logla
 * Not: Bu aşamada fiziksel->sanal eşleme yardımı yok; düşük bellek alanlarının
 *      kimlik eşlendiğini (identity map) varsayıyoruz.
 */

static inline uint8_t acpi_checksum8(const void* p, size_t len)
{
	const uint8_t* b = (const uint8_t*)p;
	uint32_t sum = 0;
	for (size_t i = 0; i < len; i++) sum += b[i];
	return (uint8_t)(sum & 0xFF);
}

static bool acpi_validate_signature(const char sig[4], const char* want)
{
	/* want 4-char string olmalı */
	return strncmp(sig, want, 4) == 0;
}

static bool acpi_validate_sdt(const acpi_sdt_header* hdr)
{
	if (!hdr) return false;
	if (hdr->Length < sizeof(acpi_sdt_header)) return false;
	return acpi_checksum8(hdr, hdr->Length) == 0;
}

typedef struct acpi_found_tables {
	const acpi_sdt_header* xsdt;
	const acpi_sdt_header* rsdt;
	const acpi_madt* madt;
	const acpi_sdt_header* fadt; /* FACP */
	const acpi_hpet* hpet;
	const acpi_sdt_header* mcfg;
} acpi_found_tables;

static acpi_found_tables g_acpi_tables; /* Basit global durum */

/* Basit getter'lar */
const acpi_sdt_header* acpi_get_xsdt(void) { return g_acpi_tables.xsdt; }
const acpi_sdt_header* acpi_get_rsdt(void) { return g_acpi_tables.rsdt; }
const acpi_madt*        acpi_get_madt(void) { return g_acpi_tables.madt; }
const acpi_sdt_header*  acpi_get_fadt(void) { return g_acpi_tables.fadt; }
const acpi_hpet*        acpi_get_hpet(void) { return g_acpi_tables.hpet; }
const acpi_sdt_header*  acpi_get_mcfg(void) { return g_acpi_tables.mcfg; }

static void acpi_scan_rsdt_xsdt(const acpi_sdt_header* root, acpi_found_tables* out)
{
	if (!root || !out) return;

	bool is_xsdt = acpi_validate_signature(root->Signature, ACPI_SIG_XSDT);
	size_t entry_size = is_xsdt ? 8 : 4;

	if (!acpi_validate_sdt(root)) {
		ERROR("ACPI: Root SDT checksum invalid (%c%c%c%c)",
			  root->Signature[0], root->Signature[1], root->Signature[2], root->Signature[3]);
		return;
	}

	size_t entries_bytes = root->Length - sizeof(acpi_sdt_header);
	size_t entry_count = entries_bytes / entry_size;
	const uint8_t* entries = (const uint8_t*)(root + 1);

	LOG("ACPI: %s found, entries=%u", is_xsdt ? "XSDT" : "RSDT", (unsigned)entry_count);

	for (size_t i = 0; i < entry_count; i++) {
		uintptr_t phys;
		if (is_xsdt) {
			phys = (uintptr_t)((const uint64_t*)entries)[i];
		} else {
			phys = (uintptr_t)((const uint32_t*)entries)[i];
		}

		const acpi_sdt_header* hdr = (const acpi_sdt_header*)(phys);
		if (!hdr) continue;
		if (!acpi_validate_sdt(hdr)) {
			WARN("ACPI: SDT checksum invalid at %p", (void*)hdr);
			continue;
		}

		if (acpi_validate_signature(hdr->Signature, ACPI_SIG_MADT)) {
			out->madt = (const acpi_madt*)hdr;
			LOG("ACPI: MADT(APIC) @ %p", (void*)hdr);
		} else if (acpi_validate_signature(hdr->Signature, ACPI_SIG_FADT)) {
			out->fadt = hdr;
			LOG("ACPI: FADT(FACP) @ %p", (void*)hdr);
		} else if (acpi_validate_signature(hdr->Signature, ACPI_SIG_HPET)) {
			out->hpet = (const acpi_hpet*)hdr;
			LOG("ACPI: HPET @ %p", (void*)hdr);
		} else if (acpi_validate_signature(hdr->Signature, ACPI_SIG_MCFG)) {
			out->mcfg = hdr;
			LOG("ACPI: MCFG @ %p", (void*)hdr);
		} else {
			// Diğer tabloları sessizce geç
		}
	}
}

void acpi_init(void)
{
	/* Temiz başlangıç */
	g_acpi_tables = (acpi_found_tables){0};

	struct multiboot_tag_new_acpi* tag_new = multiboot2_get_acpi_new();
	struct multiboot_tag_old_acpi* tag_old = multiboot2_get_acpi_old();

	if (!tag_new && !tag_old) {
		WARN("ACPI: Multiboot2 ACPI tags not found");
		return;
	}

	/* RSDP'yi seç (yeni varsa onu tercih et) */
	const void* rsdp_ptr = NULL;
	bool rsdp_is_v2 = false;

	if (tag_new) {
		rsdp_ptr = (const void*)(uintptr_t)&tag_new->rsdp[0];
	} else if (tag_old) {
		rsdp_ptr = (const void*)(uintptr_t)&tag_old->rsdp[0];
	}

	if (!rsdp_ptr) {
		ERROR("ACPI: RSDP pointer is NULL");
		return;
	}

	/* Önce ilk 20 bayt (v1) checksum doğrula */
	const acpi_rsdp_v1* rsdp_v1 = (const acpi_rsdp_v1*)rsdp_ptr;
	if (strncmp(rsdp_v1->Signature, ACPI_SIG_RSDP, 8) != 0) {
		ERROR("ACPI: RSDP signature invalid");
		return;
	}

	if (acpi_checksum8(rsdp_v1, sizeof(acpi_rsdp_v1)) != 0) {
		ERROR("ACPI: RSDP v1 checksum failed");
		return;
	}

	/* v2+ ise extended checksum kontrol et */
	const acpi_rsdp_v2* rsdp_v2 = (const acpi_rsdp_v2*)rsdp_ptr;
	if (rsdp_v1->Revision >= 2) {
		rsdp_is_v2 = true;
		if (rsdp_v2->Length >= sizeof(acpi_rsdp_v2)) {
			if (acpi_checksum8(rsdp_v2, rsdp_v2->Length) != 0) {
				ERROR("ACPI: RSDP v2 extended checksum failed");
				return;
			}
		}
	}

	LOG("ACPI: RSDP OK (Rev=%u, OEM=%c%c%c%c%c%c)",
		rsdp_v1->Revision,
		rsdp_v1->OEMID[0], rsdp_v1->OEMID[1], rsdp_v1->OEMID[2],
		rsdp_v1->OEMID[3], rsdp_v1->OEMID[4], rsdp_v1->OEMID[5]);

	/* Root tabloları belirle */
	const acpi_sdt_header* root = NULL;
	acpi_found_tables found = {0};

	if (rsdp_is_v2 && rsdp_v2->XsdtAddress) {
		root = (const acpi_sdt_header*)(uintptr_t)rsdp_v2->XsdtAddress;
		found.xsdt = root;
	} else if (rsdp_v1->RsdtAddress) {
		root = (const acpi_sdt_header*)(uintptr_t)rsdp_v1->RsdtAddress;
		found.rsdt = root;
	}

	if (!root) {
		ERROR("ACPI: No RSDT/XSDT address present");
		return;
	}

	/* Kök tabloyu tara ve önemli tabloları keşfet */
	acpi_scan_rsdt_xsdt(root, &found);

	/* Son durum raporu */
	LOG("ACPI: Summary -> XSDT=%p RSDT=%p MADT=%p FADT=%p HPET=%p MCFG=%p",
		(void*)found.xsdt, (void*)found.rsdt, (void*)found.madt,
		(void*)found.fadt, (void*)found.hpet, (void*)found.mcfg);

	g_acpi_tables = found; /* Global durum güncelleme */
}
