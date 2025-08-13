#!/bin/bash
# QEMU test script for UmayOS

PROJECT_NAME="UmayOS"
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

case "$BOOT_MODE" in
    "bios")
        log_info "Starting QEMU with BIOS boot"
        qemu-system-x86_64 \
            -m $MEMORY \
            -cdrom "$ISO_FILE" \
            -boot d \
            -vga std \
            -no-reboot \
            -no-shutdown \
            $DEBUG
        ;;
    "efi32")
        log_info "Starting QEMU with EFI32 boot"
        # Check if OVMF_CODE_IA32.fd exists
        OVMF_IA32="/usr/share/qemu/OVMF_CODE_IA32.fd"
        if [ ! -f "$OVMF_IA32" ]; then
            log_error "OVMF IA32 firmware not found: $OVMF_IA32"
            echo "Install: sudo apt install ovmf"
            exit 1
        fi
        
        qemu-system-i386 \
            -m $MEMORY \
            -bios "$OVMF_IA32" \
            -cdrom "$ISO_FILE" \
            -boot d \
            -vga std \
            -no-reboot \
            -no-shutdown \
            $DEBUG
        ;;
    "efi64")
        log_info "Starting QEMU with EFI64 boot"
        # Check if OVMF_CODE.fd exists
        OVMF_X64="/usr/share/qemu/OVMF_CODE.fd"
        if [ ! -f "$OVMF_X64" ]; then
            log_error "OVMF x64 firmware not found: $OVMF_X64"
            echo "Install: sudo apt install ovmf"
            exit 1
        fi
        
        qemu-system-x86_64 \
            -m $MEMORY \
            -bios "$OVMF_X64" \
            -cdrom "$ISO_FILE" \
            -boot d \
            -vga std \
            -no-reboot \
            -no-shutdown \
            $DEBUG
        ;;
    "debug")
        log_info "Starting QEMU with debug support (GDB)"
        qemu-system-x86_64 \
            -m $MEMORY \
            -cdrom "$ISO_FILE" \
            -boot d \
            -vga std \
            -no-reboot \
            -no-shutdown \
            -s -S
        ;;
    *)
        echo "Usage: $0 <boot_mode> [options]"
        echo ""
        echo "Boot modes:"
        echo "  bios     - Boot with BIOS (legacy)"
        echo "  efi32    - Boot with EFI 32-bit"
        echo "  efi64    - Boot with EFI 64-bit"
        echo "  debug    - Boot with GDB debugging support"
        echo ""
        echo "Examples:"
        echo "  $0 bios"
        echo "  $0 efi64"
        echo "  $0 debug"
        exit 1
        ;;
esac