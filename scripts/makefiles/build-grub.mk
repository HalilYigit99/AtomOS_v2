# GRUB and ISO build rules

# ISO staging directories
ISO_BOOT = $(ISO_STAGING)/boot
ISO_GRUB = $(ISO_BOOT)/grub
ISO_EFI = $(ISO_STAGING)/EFI
ISO_EFI_BOOT = $(ISO_EFI)/BOOT

# GRUB EFI binaries (these will be generated)
GRUB_EFI_X64 = $(ISO_EFI_BOOT)/bootx64.efi
GRUB_EFI_IA32 = $(ISO_EFI_BOOT)/bootia32.efi

# GRUB modules
GRUB_MODULES_COMMON = multiboot2 normal configfile ls cat echo test font gfxterm gfxmenu memrw
GRUB_MODULES_BIOS = part_msdos part_gpt fat ext2
GRUB_MODULES_EFI = efi_gop efi_uga part_gpt fat
GRUB_MODULES_VIDEO = all_video video

.PHONY: verify-grub

# Verify GRUB tools
verify-grub:
	@which grub-mkrescue >/dev/null 2>&1 || { \
		$(call log_error,grub-mkrescue not found); \
		echo "Install: sudo apt install grub-pc-bin grub-efi-amd64-bin grub-efi-ia32-bin"; \
		exit 1; \
	}
	@which grub-mkstandalone >/dev/null 2>&1 || { \
		$(call log_error,grub-mkstandalone not found); \
		echo "Install: sudo apt install grub-pc-bin grub-efi-amd64-bin grub-efi-ia32-bin"; \
		exit 1; \
	}
	$(call log_info,GRUB tools verified)

# ISO information
iso-info:
	@echo "=== ISO Build Information ==="
	@echo "Final ISO: $(FINAL_ISO)"
	@echo "Staging area: $(ISO_STAGING)"
	@echo "EFI64 binary: $(GRUB_EFI_X64)"
	@echo "EFI32 binary: $(GRUB_EFI_IA32)"
	@if [ -f "$(FINAL_ISO)" ]; then \
		echo "ISO size: $(du -h $(FINAL_ISO) | cut -f1)"; \
	fi