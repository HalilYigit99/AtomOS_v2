# AtomOS

AtomOS is a hobby multi-architecture x86 operating system that builds both 32-bit (i386) and 64-bit (x86_64) kernels into a single hybrid BIOS/UEFI ISO. The project explores modern firmware handoff, ACPI, storage, graphics, and filesystem support while keeping the build reproducible with a modular Make system.

## Highlights

- Multi-architecture boot: GRUB2 + Multiboot2 loads either `kernelx86.elf` or `kernelx64.elf` based on firmware (BIOS, EFI32, EFI64).
- Unified ISO pipeline: `make iso` produces `AtomOS.iso` with stand-alone GRUB EFI binaries plus legacy BIOS support.
- Firmware-aware kernel bring-up: parses Multiboot2 tags, initialises EFI runtime when present, validates ACPI (RSDP/XSDT, MADT, FADT, HPET, MCFG), and switches to APIC/HPET timers if available.
- Graphics pipeline: double-buffered framebuffer driver, resolution negotiation, bitmap font renderer, graphical terminal, mouse cursor, and a periodic redraw task targeting ~60 FPS.
- Input devices: PS/2 keyboard and mouse drivers plug into the driver framework and feed the GUI surface.
- Storage stack: driver abstraction with AHCI (command engine + IRQ handling) and legacy ATA PIO fallback registering block devices.
- Volume management: MBR/GPT parsing, device naming, and auto-mount to `/dev/blk*`, `/mnt/sd*`, or `/mnt/cd*` depending on media type.
- Virtual filesystem: mount-aware VFS core with path caching, RAMFS root, and read-focused filesystem drivers for FAT12/16/32, ISO9660, and NTFS, plus overlay buffers for write experiments.
- Streaming I/O helpers: disk/file/output stream abstractions decouple kernel subsystems from concrete backends.
- Debug-friendly build: UART + graphical logging, structured macros, `DEBUG=1` builds, and a GDB launcher script streamline tracing.

## Project Status

AtomOS is a work-in-progress research kernel. The 32-bit and 64-bit builds boot in QEMU, enumerate ACPI tables, switch to APIC + HPET where supported, draw a GUI surface, and mount FAT/ISO volumes exposed through AHCI or ATA emulation. User-space, scheduling, and full filesystem write-through are under active development; expect rough edges.

## Getting Started

### Host dependencies (Ubuntu/WSL example)

```bash
sudo apt update
sudo apt install build-essential nasm qemu-system-x86 xorriso ovmf
sudo apt install grub-pc-bin grub-efi-amd64-bin grub-efi-ia32-bin
```

### Cross-compilers

The build expects bare-metal cross toolchains on PATH:

- `i686-elf-gcc`, `i686-elf-ld`, `i686-elf-objcopy`, …
- `x86_64-elf-gcc`, `x86_64-elf-ld`, `x86_64-elf-objcopy`, …

You can build them with the OSDev cross-compiler guide or use prebuilt toolchains.

### Building

```bash
make            # same as `make iso`
make kernel32   # 32-bit kernel only
make kernel64   # 64-bit kernel only
make iso        # rebuild hybrid ISO
make clean      # wipe build/, iso/, AtomOS.iso
```

Useful flags and helpers:

- `DEBUG=1 make iso` keeps symbols, relaxes optimisations, and enables extra logging.
- `make info`, `make toolchain-info`, `make kernel-info`, `make iso-info` print build metadata.

Build artefacts live under `build/` (ELFs, intermediates) and `iso/` (staging tree). The final image is `AtomOS.iso` in the repository root.

### Running in QEMU

Run targets proxy to `scripts/tools/run_qemu.sh`:

```bash
make run-bios        # qemu-system-i386, BIOS boot
make run-efi64       # qemu-system-x86_64 with OVMF.fd
make run-efi32       # requires /usr/share/qemu/OVMF_CODE_IA32.fd
make run-debug       # BIOS boot + GDB stub (TCP :1234)
make run-efi64-debug # EFI64 boot + GDB stub
```

You can call the script directly:

```bash
./scripts/tools/run_qemu.sh bios
./scripts/tools/run_qemu.sh efi64
./scripts/tools/run_qemu.sh efi32-debug
```

### Debugging

- `DEBUG=1 make run-debug` starts QEMU halted with a GDB server.
- `scripts/tools/gdb_wrap.sh <arch> <elf>` chooses an appropriate GDB (`gdb-multiarch` if available) and skips noisy symbols.
  - `scripts/tools/gdb_wrap.sh i386 build/kernelx86.elf`
  - `scripts/tools/gdb_wrap.sh x86_64 build/kernelx64.elf`
- Logs stream to the graphical terminal, UART, and serial stdio depending on boot method.

## Boot Flow Overview

1. GRUB2 (BIOS or stand-alone EFI) loads either `kernelx86.elf` or `kernelx64.elf` via Multiboot2.
2. Early assembly (`kernel/*/start.asm`) sets up CPU state, stacks, and jumps into `boot/boot.c`.
3. Multiboot2 tags are parsed for memory maps, framebuffer, ACPI, and EFI handles.
4. Heap and physical memory managers come online; EFI runtime or BIOS shims initialise firmware services.
5. ACPI tables are validated; APIC/HPET drivers replace PIC/PIT when available.
6. Graphics initialises framebuffer targets, picks the best sub-1080p mode, and spawns a periodic task for redraw.
7. Driver framework registers input, timer, and storage drivers (AHCI first, ATA fallback).
8. Block devices register with the volume manager; MBR/GPT volumes mount into `/mnt`.
9. VFS mounts RAMFS as `/`, then auto-mounts detected filesystems under `/dev/` and `/mnt/`.
10. `kmain()` demonstrates filesystem usage by creating a file on a mounted volume and dumping directory listings.

## Subsystems

- **Memory:** Boot-time PMM builds from firmware maps, a growable heap backs `malloc/calloc`, and alignment helpers support DMA-friendly allocations.
- **Driver framework:** `DriverBase` offers register/enable hooks shared by APIC, PIC, HPET, PIT, PS/2 devices, AHCI, ATA, VBE, EFI GOP, and more.
- **Graphics:** `graphics/` provides buffer management, bitmap font rasteriser, BMP loader, and screen abstraction; `gfxterm/` implements a text terminal over the framebuffer.
- **Filesystem stack:** `filesystem/VFS.c` implements a cached, mount-aware VFS with path normalisation and stream-backed file handles. Drivers live under `filesystem/{ramfs,fat,iso9660,ntfs}`.
- **Storage:** `storage/BlockDevice.c` abstracts read/write/flush while `storage/VolumeManager.c` discovers partitions (MBR & GPT) and associates them with filesystems.
- **Testing utilities:** `kernel/tests/` hosts diagnostics such as `block_read_test` for exercising the block device layer.
- **Streams & logging:** `stream/OutputStream`, `FileStream`, and `DiskStream` unify I/O, while `debug/` covers UART, exceptions, and the graphics debugger terminal.

## Repository Layout

```
.
├── Makefile                     # Top-level build orchestration
├── scripts/
│   ├── makefiles/               # Modular build rules (arch, kernel, libc, grub, toolchain)
│   ├── tools/                   # QEMU, GDB, ISO helper scripts
│   └── grub.cfg                 # Smart GRUB2 config used in the ISO
├── kernel/
│   ├── boot/                    # Multiboot2 entry, early init
│   ├── i386/, amd64/            # Arch-specific assembly and paging code
│   ├── driver/                  # AHCI, ATA, APIC, PIC, HPET, PS/2, VBE, EFI GOP, ...
│   ├── graphics/, gfxterm/      # Framebuffer renderer and terminal
│   ├── filesystem/              # VFS core + RAMFS/FAT/ISO9660/NTFS drivers
│   ├── storage/                 # Block device registry and volume manager
│   ├── acpi/, efi/, bios/       # Firmware integration layers
│   ├── debug/, irq/, time/, task/ # Diagnostics, interrupt routing, timers, periodic tasks
│   └── kmain.c                  # Kernel entry point
├── kernel_include/              # Public headers shared across architectures
├── libc/, libc_include/         # Planned userland libc (currently stubs)
├── build/                       # Generated object files and ELF binaries
├── iso/                         # ISO staging tree populated by `make iso`
├── AtomOS.iso                   # Final hybrid image after a build
└── logo*.bmp                    # Branding assets displayed at boot
```

## Testing

Kernel-side tests live in `kernel/tests/`. To try the block device reader example:

1. Include `tests/block_read_test.h` in `kmain.c`.
2. Call `block_read_test_run()` after storage drivers initialise.
3. Boot via QEMU and observe hexdumps from LBA 0/1.

Additional harnesses can be added under `kernel/tests/` and invoked from boot code or debug builds.

## Contributing

AtomOS is currently developed in a single repository. If you plan to contribute, open an issue or PR describing the subsystem you are touching so we can coordinate interfaces (especially around the VFS and driver frameworks).

## License

The project has not published a formal license yet; all rights reserved until one is added.
