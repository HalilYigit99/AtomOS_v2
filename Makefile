# UmayOS Main Makefile
PROJECT_NAME = UmayOS

# Default target (must be first!)
all: iso

# Include modular makefiles
include scripts/makefiles/common.mk
include scripts/makefiles/toolchain.mk
include scripts/makefiles/build-kernel.mk
include scripts/makefiles/build-libc.mk
include scripts/makefiles/build-grub.mk

.PHONY: all clean kernel libc iso info

# Build kernels for both architectures
kernel: kernel32 kernel64

# Build LibC for both architectures (future)
libc: libc32 libc64

# Build final ISO with proper dependencies
iso: kernel
	$(call log_build,Setting up ISO staging area)
	@mkdir -p $(ISO_STAGING)/boot $(ISO_STAGING)/boot/grub $(ISO_STAGING)/EFI/BOOT
	@mkdir -p $(ISO_STAGING)/boot/grub/fonts
	@cp $(KERNEL_X86_ELF) $(ISO_STAGING)/boot/kernelx86.elf
	@cp $(KERNEL_X64_ELF) $(ISO_STAGING)/boot/kernelx64.elf
	@cp $(SCRIPTS_DIR)/grub.cfg $(ISO_STAGING)/boot/grub/grub.cfg
	@# Copy GRUB font if available
	@if [ -f /usr/share/grub/unicode.pf2 ]; then \
		cp /usr/share/grub/unicode.pf2 $(ISO_STAGING)/boot/grub/fonts/; \
	elif [ -f /boot/grub/fonts/unicode.pf2 ]; then \
		cp /boot/grub/fonts/unicode.pf2 $(ISO_STAGING)/boot/grub/fonts/; \
	else \
		$(call log_warn,Unicode font not found - ASCII characters may display incorrectly); \
	fi
	$(call log_info,ISO staging area prepared)
	$(call log_build,Creating GRUB EFI binaries)
	@# Create EFI64 bootloader
	@grub-mkstandalone \
		--format=x86_64-efi \
		--output=$(ISO_STAGING)/EFI/BOOT/bootx64.efi \
		--modules="multiboot2 normal configfile ls cat echo test font gfxterm gfxmenu efi_gop efi_uga part_gpt fat all_video video" \
		boot/grub/grub.cfg=$(SCRIPTS_DIR)/grub.cfg
	@# Create EFI32 bootloader
	@grub-mkstandalone \
		--format=i386-efi \
		--output=$(ISO_STAGING)/EFI/BOOT/bootia32.efi \
		--modules="multiboot2 normal configfile ls cat echo test font gfxterm gfxmenu efi_gop efi_uga part_gpt fat all_video video" \
		boot/grub/grub.cfg=$(SCRIPTS_DIR)/grub.cfg
	$(call log_info,GRUB EFI binaries created)
	$(call log_build,Creating hybrid BIOS/EFI bootable ISO)
	@$(GRUB_MKRESCUE) \
		--output=$(FINAL_ISO) \
		--modules="multiboot2 normal configfile ls cat echo test font gfxterm gfxmenu part_msdos part_gpt fat ext2" \
		$(ISO_STAGING)
	$(call log_info,ISO created: $(FINAL_ISO))

# Information targets
info:
	@echo "=== $(PROJECT_NAME) Build Information ==="
	@echo "Available targets:"
	@echo "  all       - Build complete ISO"
	@echo "  kernel    - Build both kernel architectures"
	@echo "  kernel32  - Build 32-bit kernel only"
	@echo "  kernel64  - Build 64-bit kernel only"
	@echo "  libc      - Build LibC for both architectures"
	@echo "  iso       - Create bootable ISO"
	@echo "  clean     - Clean all build files"
	@echo "  info      - Show this information"

# Clean everything
clean:
	@echo "Cleaning build directory..."
	rm -rf build/
	rm -rf iso/
	rm -f $(PROJECT_NAME).iso
	@echo "Clean completed."