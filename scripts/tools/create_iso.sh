#!/bin/bash
# ISO creation script for AtomOS

PROJECT_NAME="AtomOS"
ISO_STAGING="iso"
BUILD_DIR="build"
FINAL_ISO="${PROJECT_NAME}.iso"

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_build() {
    echo -e "${BLUE}[BUILD]${NC} $1"
}

# Check if kernels exist
check_kernels() {
    if [ ! -f "$BUILD_DIR/kernelx86.elf" ]; then
        log_error "32-bit kernel not found: $BUILD_DIR/kernelx86.elf"
        echo "Run 'make kernel32' first"
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/kernelx64.elf" ]; then
        log_error "64-bit kernel not found: $BUILD_DIR/kernelx64.elf"
        echo "Run 'make kernel64' first"
        exit 1
    fi
    
    log_info "Both kernels found"
}

# Check required tools
check_tools() {
    local missing_tools=()
    
    if ! command -v grub-mkrescue >/dev/null 2>&1; then
        missing_tools+=("grub-mkrescue")
    fi
    
    if ! command -v grub-mkstandalone >/dev/null 2>&1; then
        missing_tools+=("grub-mkstandalone")
    fi
    
    if ! command -v xorriso >/dev/null 2>&1; then
        missing_tools+=("xorriso")
    fi
    
    if [ ${#missing_tools[@]} -ne 0 ]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        echo "Install with: sudo apt install grub-pc-bin grub-efi-amd64-bin grub-efi-ia32-bin xorriso"
        exit 1
    fi
    
    log_info "All required tools found"
}

# Setup ISO staging directory
setup_staging() {
    log_build "Setting up ISO staging directory"
    
    # Create directory structure
    mkdir -p "$ISO_STAGING/boot/grub"
    mkdir -p "$ISO_STAGING/EFI/BOOT"
    
    # Copy kernels
    cp "$BUILD_DIR/kernelx86.elf" "$ISO_STAGING/boot/"
    cp "$BUILD_DIR/kernelx64.elf" "$ISO_STAGING/boot/"
    
    # Copy GRUB configuration
    cp "scripts/grub.cfg" "$ISO_STAGING/boot/grub/"
    
    log_info "ISO staging directory prepared"
}

# Create EFI bootloaders
create_efi_bootloaders() {
    log_build "Creating EFI bootloaders"
    
    # Create EFI64 bootloader
    grub-mkstandalone \
        --format=x86_64-efi \
        --output="$ISO_STAGING/EFI/BOOT/bootx64.efi" \
        --modules="multiboot2 normal configfile ls cat echo test efi_gop efi_uga part_gpt fat all_video" \
        boot/grub/grub.cfg=scripts/grub.cfg
    
    if [ $? -ne 0 ]; then
        log_error "Failed to create EFI64 bootloader"
        exit 1
    fi
    
    # Create EFI32 bootloader
    grub-mkstandalone \
        --format=i386-efi \
        --output="$ISO_STAGING/EFI/BOOT/bootia32.efi" \
        --modules="multiboot2 normal configfile ls cat echo test efi_gop efi_uga part_gpt fat all_video" \
        boot/grub/grub.cfg=scripts/grub.cfg
    
    if [ $? -ne 0 ]; then
        log_error "Failed to create EFI32 bootloader"
        exit 1
    fi
    
    log_info "EFI bootloaders created"
}

# Create hybrid ISO
create_iso() {
    log_build "Creating hybrid BIOS/EFI ISO"
    
    grub-mkrescue \
        --output="$FINAL_ISO" \
        --modules="multiboot2 normal configfile ls cat echo test biosdisk part_msdos part_gpt fat ext2 efi_gop efi_uga all_video vbe vga" \
        --verbose \
        "$ISO_STAGING"
    
    if [ $? -ne 0 ]; then
        log_error "Failed to create ISO"
        exit 1
    fi
    
    # Show ISO information
    local iso_size=$(du -h "$FINAL_ISO" | cut -f1)
    log_info "ISO created successfully: $FINAL_ISO ($iso_size)"
}

# Verify ISO contents
verify_iso() {
    log_build "Verifying ISO contents"
    
    # Check if ISO file exists and has reasonable size
    if [ ! -f "$FINAL_ISO" ]; then
        log_error "ISO file was not created"
        exit 1
    fi
    
    local size_bytes=$(stat -f%z "$FINAL_ISO" 2>/dev/null || stat -c%s "$FINAL_ISO" 2>/dev/null)
    if [ "$size_bytes" -lt 1000000 ]; then  # Less than 1MB is suspicious
        log_warn "ISO file seems very small ($size_bytes bytes)"
    fi
    
    log_info "ISO verification completed"
}

# Main execution
main() {
    echo "=== AtomOS ISO Creation ==="
    
    check_tools
    check_kernels
    setup_staging
    create_efi_bootloaders
    create_iso
    verify_iso
    
    echo ""
    echo "=== ISO Creation Complete ==="
    echo "File: $FINAL_ISO"
    echo "Test with:"
    echo "  ./scripts/tools/run_qemu.sh bios"
    echo "  ./scripts/tools/run_qemu.sh efi64"
}

# Run main function
main "$@"