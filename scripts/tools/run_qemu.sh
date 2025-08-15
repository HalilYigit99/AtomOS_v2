#!/bin/bash
# QEMU test script for AtomOS

PROJECT_NAME="AtomOS"
ISO_FILE="${PROJECT_NAME}.iso"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Check if ISO exists
if [ ! -f "$ISO_FILE" ]; then
    log_error "ISO file not found: $ISO_FILE"
    echo "Run 'make iso' first to build the ISO"
    exit 1
fi

# Parse arguments
BOOT_MODE="$1"
MEMORY="512M"
DEBUG=""

run_qemu() {
    # $1 = cmd (string)
    eval "$1"
}

case "$BOOT_MODE" in
    bios)
        log_info "Starting QEMU with BIOS boot (32-bit)"
        run_qemu "qemu-system-i386 -m $MEMORY -cdrom '$ISO_FILE' -serial stdio -boot d -vga std $DEBUG" ;;
    bios-debug)
        log_info "Starting QEMU (BIOS debug, waiting for GDB on :1234, 32-bit)"
        run_qemu "qemu-system-i386 -m $MEMORY -cdrom '$ISO_FILE' -boot d -vga std -s -S $DEBUG" ;;
    efi32)
        log_info "Starting QEMU with EFI32 boot"
        OVMF_IA32="/usr/share/qemu/OVMF_CODE_IA32.fd"
        if [ ! -f "$OVMF_IA32" ]; then
            log_error "OVMF IA32 firmware not found: $OVMF_IA32"; echo "Install: sudo apt install ovmf"; exit 1; fi
    run_qemu "qemu-system-i386 -m $MEMORY -bios '$OVMF_IA32' -serial stdio -cdrom '$ISO_FILE' -boot d -vga std $DEBUG" ;;
    efi32-debug)
        log_info "Starting QEMU (EFI32 debug, waiting for GDB on :1234)"
        OVMF_IA32="/usr/share/qemu/OVMF_CODE_IA32.fd"
        if [ ! -f "$OVMF_IA32" ]; then
            log_error "OVMF IA32 firmware not found: $OVMF_IA32"; echo "Install: sudo apt install ovmf"; exit 1; fi
    run_qemu "qemu-system-i386 -m $MEMORY -bios '$OVMF_IA32' -cdrom '$ISO_FILE' -boot d -vga std -s -S $DEBUG" ;;
    efi64)
        log_info "Starting QEMU with EFI64 boot"
        OVMF_X64="/usr/share/qemu/OVMF.fd"
        if [ ! -f "$OVMF_X64" ]; then
            log_error "OVMF x64 firmware not found: $OVMF_X64"; echo "Install: sudo apt install ovmf"; exit 1; fi
    run_qemu "qemu-system-x86_64 -m $MEMORY -bios '$OVMF_X64' -cdrom '$ISO_FILE' -serial stdio -boot d -vga std $DEBUG" ;;
    efi64-debug)
        log_info "Starting QEMU (EFI64 debug, waiting for GDB on :1234)"
        OVMF_X64="/usr/share/qemu/OVMF.fd"
        if [ ! -f "$OVMF_X64" ]; then
            log_error "OVMF x64 firmware not found: $OVMF_X64"; echo "Install: sudo apt install ovmf"; exit 1; fi
    run_qemu "qemu-system-x86_64 -m $MEMORY -bios '$OVMF_X64' -cdrom '$ISO_FILE' -boot d -vga std -s -S $DEBUG" ;;
    *)
        echo "Usage: $0 <boot_mode>"; echo "";
        echo "Boot modes:";
        echo "  bios | bios-debug";
        echo "  efi32 | efi32-debug";
        echo "  efi64 | efi64-debug";
        exit 1 ;;
esac