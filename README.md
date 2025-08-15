# AtomOS

A multi-architecture operating system supporting both 32-bit (i386) and 64-bit (x86_64) architectures with hybrid BIOS/EFI boot support.

## Features

- **Multi-Architecture Support**: Builds both 32-bit and 64-bit kernels simultaneously
- **Universal Boot**: Single ISO boots on BIOS, EFI32, and EFI64 systems
- **Smart GRUB Configuration**: Automatically selects appropriate kernel based on boot method
- **Modular Build System**: Clean separation of architecture-specific and independent code
- **NASM Assembly**: Uses NASM for all assembly code compilation
- **Cross-Compilation**: Full cross-compilation toolchain support

## Build Requirements

### Ubuntu/WSL Dependencies

```bash
sudo apt update
sudo apt install build-essential nasm qemu-system-x86
sudo apt install grub-pc-bin grub-efi-amd64-bin grub-efi-ia32-bin
sudo apt install xorriso ovmf
```

### Cross-Compilation Toolchain

You need to install cross-compilers for both architectures:

- `i686-elf-gcc` for 32-bit compilation
- `x86_64-elf-gcc` for 64-bit compilation

## Project Structure

```
AtomOS/
├── kernel/
│   ├── i386/           # 32-bit architecture specific code
│   ├── amd64/          # 64-bit architecture specific code
│   └── [other dirs]    # Architecture-independent kernel code
├── kernel_include/     # Kernel headers
├── libc/              # LibC implementation (for user-space)
├── libc_include/      # LibC headers
├── scripts/           # Build system and configuration
│   ├── makefiles/     # Modular Makefiles
│   ├── tools/         # Utility scripts
│   ├── grub.cfg       # GRUB configuration
│   └── linker.ld      # Linker script
└── build/             # Build outputs
    ├── kernelx86.elf  # 32-bit kernel
    └── kernelx64.elf  # 64-bit kernel
```

## Building

### Build Everything

```bash
make all
```

This creates `AtomOS.iso` - a hybrid ISO that boots on all supported systems.

### Build Specific Components

```bash
make kernel     # Build both kernels
make kernel32   # Build 32-bit kernel only  
make kernel64   # Build 64-bit kernel only
make libc       # Build LibC (future)
make iso        # Create bootable ISO
```

### Cleaning

```bash
make clean      # Remove all build files
```

## Testing

### QEMU Testing

```bash
# Test different boot methods
./scripts/tools/run_qemu.sh bios     # BIOS boot
./scripts/tools/run_qemu.sh efi32    # EFI 32-bit boot
./scripts/tools/run_qemu.sh efi64    # EFI 64-bit boot
./scripts/tools/run_qemu.sh debug    # Debug with GDB
```

### Build Information

```bash
make info           # Show build targets
make toolchain-info # Show toolchain details
make kernel-info    # Show kernel build details
```

## Architecture Design

### Build Output Organization

- `build/x86/` - Architecture-specific code compiled for 32-bit
- `build/x86_64/` - Architecture-specific code compiled for 64-bit  
- `build/kernel_x86/` - Architecture-independent code compiled for 32-bit
- `build/kernel_x86_64/` - Architecture-independent code compiled for 64-bit

### Boot Process

1. **GRUB Detection**: Automatically detects boot method (BIOS/EFI32/EFI64)
2. **Kernel Selection**: Loads appropriate kernel based on platform capabilities
3. **Smart Configuration**: Presents relevant menu options only

### Kernel Linking

- **32-bit kernel**: Links `build/x86/*.o` + `build/kernel_x86/*.o` → `kernelx86.elf`
- **64-bit kernel**: Links `build/x86_64/*.o` + `build/kernel_x86_64/*.o` → `kernelx64.elf`

## Development

The build system is designed for easy development:

- **Modular Makefiles**: Each component has its own build rules
- **Automatic Dependencies**: Source files are discovered automatically
- **Clean Separation**: Architecture-specific vs independent code
- **Cross-Platform**: Works on Ubuntu, WSL, and other Linux distributions

## License

[Add your license here]

## Contributing

[Add contribution guidelines here]