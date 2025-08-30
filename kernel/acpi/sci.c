#include <acpi/acpi.h>
#include <acpi/acpi_new.h>
#include <acpi/acpi_old.h>
#include <debug/debug.h>
#include <irq/IRQ.h>
#include <arch.h>

// NOTE: User's I/O helpers use signature: outb(port, value)

void sci_isr();         // Assembly entry for SCI IRQ
void sci_isr_handler(); // C handler called by sci_isr

static uint16_t sci_irq_number = 0;

static inline void acpi_enable_if_needed(ACPI_FADT *fadt) {
    // Send ACPI_ENABLE to SMI_CMD and wait until SCI_EN becomes 1
    if (fadt->SmiCommandPort && fadt->AcpiEnable) {
        outb((uint16_t)fadt->SmiCommandPort, (uint8_t)fadt->AcpiEnable);
        LOG("ACPI: Sent ACPI_ENABLE=0x%X to SMI_CMD=0x%X", fadt->AcpiEnable, fadt->SmiCommandPort);

        // Wait for SCI_EN (bit0 of PM1 Control) to be set
        if (fadt->Pm1aControlBlock && fadt->Pm1ControlLength >= 2) {
            volatile uint16_t *pm1a_cnt = (uint16_t *)(uintptr_t)fadt->Pm1aControlBlock;
            // Simple bounded spin-wait to avoid infinite loop on buggy FW
            for (uint32_t i = 0; i < 1000000; ++i) {
                if ((*pm1a_cnt) & 0x0001) {
                    LOG("ACPI: SCI_EN became 1 (PM1a_CNT=0x%04X)", *pm1a_cnt);
                    break;
                }
            }
        }
    } else {
        WARN("ACPI: SMI_CMD or ACPI_ENABLE not available — assuming ACPI already enabled by firmware");
    }
}

static inline void acpi_enable_events(ACPI_FADT *fadt) {
    // Enable PWRBTN_EN and SLPBTN_EN in PM1 Event Enable register(s)
    ASSERT(fadt->Pm1aEventBlock && fadt->Pm1EventLength >= 4,
           "ACPI PM1a Event Block is not available or length < 4 (need STS+EN)");

    volatile uint16_t *pm1a_en = (uint16_t *)(uintptr_t)(fadt->Pm1aEventBlock + 2); // EN = STS+2
    uint16_t val_a = *pm1a_en;
    val_a |= (1u << 8) | (1u << 9); // PWRBTN_EN (bit8), SLPBTN_EN (bit9)
    *pm1a_en = val_a;

    LOG("ACPI: PM1a_EN at %p set -> 0x%04X", pm1a_en, val_a);

    if (fadt->Pm1bEventBlock) {
        // Optional PM1b block
        volatile uint16_t *pm1b_en = (uint16_t *)(uintptr_t)(fadt->Pm1bEventBlock + 2);
        uint16_t val_b = *pm1b_en;
        val_b |= (1u << 8) | (1u << 9);
        *pm1b_en = val_b;
        LOG("ACPI: PM1b_EN at %p set -> 0x%04X", pm1b_en, val_b);
    }
}

void acpi_sci_init() {
    ASSERT(acpi_fadt_ptr != NULL, "ACPI FADT pointer is NULL");
    ACPI_FADT *fadt = acpi_fadt_ptr;

    // Remember SCI IRQ number first
    sci_irq_number = fadt->SciInterrupt;
    LOG("ACPI: SCI Interrupt line = IRQ %u", sci_irq_number);

    // Route/prepare IRQ in the controller
    ASSERT(irq_controller != NULL, "IRQ controller is NULL");
    irq_controller->register_handler(sci_irq_number, sci_isr);
    irq_controller->enable(sci_irq_number);

    // Put ACPI into ACPI mode and wait for SCI_EN
    acpi_enable_if_needed(fadt);

    // Enable the events that should generate SCI
    acpi_enable_events(fadt);

    LOG("ACPI: SCI initialization complete");
}

static inline void acpi_ack_and_clear(ACPI_FADT *fadt) {
    // Read PM1 status and clear W1C bits for handled events
    volatile uint16_t *pm1a_sts = (uint16_t *)(uintptr_t)fadt->Pm1aEventBlock; // STS
    uint16_t sts_a = *pm1a_sts;

    uint16_t clear_mask_a = 0;
    if (sts_a & (1u << 8)) { // PWRBTN_STS
        clear_mask_a |= (1u << 8);
        LOG("ACPI: Power Button event (PM1a_STS=0x%04X)", sts_a);
    }
    if (sts_a & (1u << 9)) { // SLPBTN_STS
        clear_mask_a |= (1u << 9);
        LOG("ACPI: Sleep Button event (PM1a_STS=0x%04X)", sts_a);
    }

    if (clear_mask_a) {
        *pm1a_sts = clear_mask_a; // write-1-to-clear
    }

    if (fadt->Pm1bEventBlock) {
        volatile uint16_t *pm1b_sts = (uint16_t *)(uintptr_t)fadt->Pm1bEventBlock;
        uint16_t sts_b = *pm1b_sts;
        uint16_t clear_mask_b = 0;
        if (sts_b & (1u << 8)) clear_mask_b |= (1u << 8);
        if (sts_b & (1u << 9)) clear_mask_b |= (1u << 9);
        if (clear_mask_b) {
            *pm1b_sts = clear_mask_b; // write-1-to-clear
        }
    }
}

void sci_isr_handler() {
    ACPI_FADT *fadt = acpi_fadt_ptr;

    // For level-triggered SCI: clear status in device FIRST, then send EOI
    acpi_ack_and_clear(fadt);

    // Notify the IRQ controller (PIC/APIC) after clearing W1C bits
    irq_controller->acknowledge(sci_irq_number);

    LOG("ACPI: SCI interrupt handled (IRQ%u)", sci_irq_number);
}
