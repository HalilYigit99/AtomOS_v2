# Toolchain configuration

# Cross-compiler prefixes
CC_PREFIX_32 = i686-elf
CC_PREFIX_64 = x86_64-elf

# Tools for 32-bit
CC_32 = $(CC_PREFIX_32)-gcc
LD_32 = $(CC_PREFIX_32)-ld
OBJCOPY_32 = $(CC_PREFIX_32)-objcopy
OBJDUMP_32 = $(CC_PREFIX_32)-objdump
AR_32 = $(CC_PREFIX_32)-ar

# Tools for 64-bit
CC_64 = $(CC_PREFIX_64)-gcc
LD_64 = $(CC_PREFIX_64)-ld
OBJCOPY_64 = $(CC_PREFIX_64)-objcopy
OBJDUMP_64 = $(CC_PREFIX_64)-objdump
AR_64 = $(CC_PREFIX_64)-ar

# Common tools
AS = nasm
GRUB_MKRESCUE = grub-mkrescue

# Compiler flags for 32-bit
CFLAGS_32_BASE = -m32 -march=i686 -ffreestanding -fno-stack-protector -fno-pie -g
CFLAGS_32_WARN = -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS_32_INCLUDES = -Ikernel_include -Ilibc_include
CFLAGS_32 = $(CFLAGS_32_BASE) $(CFLAGS_32_WARN) $(CFLAGS_32_INCLUDES)

# Compiler flags for 64-bit
CFLAGS_64_BASE = -m64 -mcmodel=kernel -mno-red-zone -ffreestanding -fno-stack-protector -fno-pie -g
CFLAGS_64_WARN = -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS_64_INCLUDES = -Ikernel_include -Ilibc_include
CFLAGS_64 = $(CFLAGS_64_BASE) $(CFLAGS_64_WARN) $(CFLAGS_64_INCLUDES)

# NASM flags
NASMFLAGS_32 = -f elf32 -g -F dwarf
NASMFLAGS_64 = -f elf64 -g -F dwarf

# Linker flags
LDFLAGS_32 = -m elf_i386 -T $(SCRIPTS_DIR)/linker.ld -nostdlib
LDFLAGS_64 = -m elf_x86_64 -T $(SCRIPTS_DIR)/linker.ld -nostdlib

# Debug/Release mode
ifdef DEBUG
	CFLAGS_32 += -g -DDEBUG -fno-omit-frame-pointer
	CFLAGS_64 += -g -DDEBUG -fno-omit-frame-pointer
else
    CFLAGS_32 += -O2 -DNDEBUG
    CFLAGS_64 += -O2 -DNDEBUG
endif

# Toolchain verification
define verify_toolchain
	@echo "Verifying cross-compilation toolchain..."
	@which $(CC_32) >/dev/null 2>&1 || { \
		$(call log_error,$(CC_32) not found); \
		echo "Install i686-elf-gcc cross-compiler"; \
		exit 1; \
	}
	@which $(CC_64) >/dev/null 2>&1 || { \
		$(call log_error,$(CC_64) not found); \
		echo "Install x86_64-elf-gcc cross-compiler"; \
		exit 1; \
	}
	@which $(AS) >/dev/null 2>&1 || { \
		$(call log_error,$(AS) not found); \
		echo "Install NASM assembler: sudo apt install nasm"; \
		exit 1; \
	}
	@which $(GRUB_MKRESCUE) >/dev/null 2>&1 || { \
		$(call log_error,$(GRUB_MKRESCUE) not found); \
		echo "Install GRUB tools: sudo apt install grub-pc-bin grub-efi-amd64-bin"; \
		exit 1; \
	}
	$(call log_info,All required tools found)
endef

# Show toolchain information
toolchain-info:
	@echo "=== Toolchain Information ==="
	@echo "32-bit Compiler: $(CC_32)"
	@echo "64-bit Compiler: $(CC_64)"
	@echo "Assembler: $(AS)"
	@echo "GRUB: $(GRUB_MKRESCUE)"
	@echo "32-bit CFLAGS: $(CFLAGS_32)"
	@echo "64-bit CFLAGS: $(CFLAGS_64)"