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

.PHONY: all clean kernel libc iso info run run-bios run-efi run-efi32 run-efi64
.PHONY: all clean kernel libc iso info run run-bios run-efi run-efi32 run-efi64 \
	run-debug run-bios-debug run-efi-debug run-efi32-debug run-efi64-debug

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
	@echo "  run       - Run (alias of run-bios)"
	@echo "  run-bios  - Run ISO in QEMU (legacy BIOS)"
	@echo "  run-efi   - Run (alias of run-efi64)"
	@echo "  run-efi32 - Run ISO in QEMU using 32-bit EFI (requires IA32 OVMF)"
	@echo "  run-efi64 - Run ISO in QEMU using 64-bit EFI"
	@echo "  run-debug        - (alias run-bios-debug)"
	@echo "  run-bios-debug   - BIOS debug (QEMU -s -S + GDB attach)"
	@echo "  run-efi-debug    - (alias run-efi64-debug)"
	@echo "  run-efi32-debug  - EFI32 debug"
	@echo "  run-efi64-debug  - EFI64 debug"
	@echo "  clean     - Clean all build files"
	@echo "  info      - Show this information"
	@echo ""
	@echo "Hints: make run-efi32 için /usr/share/qemu/OVMF_CODE_IA32.fd gerekir; yoksa 32-bit OVMF paketini/manual edin."

# QEMU run targets
QEMU_RUN_SCRIPT = $(SCRIPTS_DIR)/tools/run_qemu.sh

# Varsayilan run BIOS
run: run-bios

run-bios: iso
	$(call log_info,Starting QEMU (BIOS))
	@$(QEMU_RUN_SCRIPT) bios

# EFI alias
run-efi: run-efi64

run-efi64: iso
	$(call log_info,Starting QEMU (EFI 64-bit))
	@$(QEMU_RUN_SCRIPT) efi64

run-efi32: iso
	$(call log_info,Starting QEMU (EFI 32-bit))
	@$(QEMU_RUN_SCRIPT) efi32 || { \
		$(call log_warn,EFI32 çalışmadı; 32-bit OVMF mevcut mu?); \
		echo "Eksikse: sudo apt install ovmf (yalnızca x64 sağlıyorsa ayrı IA32 firmware indirin)"; \
		echo "Alternatif: https://github.com/tianocore/edk2 'den IA32 OVMF derleyin"; \
		exit 1; \
	}

# Debug aliases
run-debug: run-bios-debug
run-efi-debug: run-efi64-debug

# Debug run targets (yeniden derlemede DEBUG=1 kullan)
run-bios-debug: DEBUG=1
run-bios-debug: iso
	$(call log_info,Launching BIOS debug session)
	@$(QEMU_RUN_SCRIPT) bios-debug & \
	sleep 1; \
	$(SCRIPTS_DIR)/tools/gdb_wrap.sh i386 $(KERNEL_X86_ELF)

run-efi32-debug: DEBUG=1
run-efi32-debug: iso
	$(call log_info,Launching EFI32 debug session)
	@$(QEMU_RUN_SCRIPT) efi32-debug & \
	sleep 1; \
	$(SCRIPTS_DIR)/tools/gdb_wrap.sh i386 $(KERNEL_X86_ELF)

run-efi64-debug: DEBUG=1
run-efi64-debug: iso
	$(call log_info,Launching EFI64 debug session)
	@$(QEMU_RUN_SCRIPT) efi64-debug & \
	sleep 1; \
	$(SCRIPTS_DIR)/tools/gdb_wrap.sh x86_64 $(KERNEL_X64_ELF)

# Clean everything
clean:
	@echo "Cleaning build directory..."
	rm -rf build/
	rm -rf iso/
	rm -f $(PROJECT_NAME).iso
	@echo "Clean completed."