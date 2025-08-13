# Common definitions and utilities
PROJECT_NAME = UmayOS
VERSION = 0.1.0

# Directory structure
BUILD_ROOT = build
ISO_STAGING = iso
KERNEL_DIR = kernel
LIBC_DIR = libc
SCRIPTS_DIR = scripts

# Build output directories
BUILD_X86 = $(BUILD_ROOT)/x86
BUILD_X86_64 = $(BUILD_ROOT)/x86_64
BUILD_KERNEL_X86 = $(BUILD_ROOT)/kernel_x86
BUILD_KERNEL_X86_64 = $(BUILD_ROOT)/kernel_x86_64
BUILD_LIBC_X86 = $(BUILD_ROOT)/libc_x86
BUILD_LIBC_X86_64 = $(BUILD_ROOT)/libc_x86_64

# Final outputs
KERNEL_X86_ELF = $(BUILD_ROOT)/kernelx86.elf
KERNEL_X64_ELF = $(BUILD_ROOT)/kernelx64.elf
LIBC_X86_SO = $(BUILD_ROOT)/libc_x86.so
LIBC_X64_SO = $(BUILD_ROOT)/libc_x64.so
FINAL_ISO = $(PROJECT_NAME).iso

# Colors for pretty output
GREEN = \033[0;32m
RED = \033[0;31m
YELLOW = \033[1;33m
BLUE = \033[0;34m
NC = \033[0m

# Utility functions
define log_info
	@echo "$(GREEN)[INFO]$(NC) $(1)"
endef

define log_warn
	@echo "$(YELLOW)[WARN]$(NC) $(1)"
endef

define log_error
	@echo "$(RED)[ERROR]$(NC) $(1)"
endef

define log_build
	@echo "$(BLUE)[BUILD]$(NC) $(1)"
endef

# Directory creation function
define create_build_dirs
	@mkdir -p $(BUILD_X86) $(BUILD_X86_64)
	@mkdir -p $(BUILD_KERNEL_X86) $(BUILD_KERNEL_X86_64)
	@mkdir -p $(BUILD_LIBC_X86) $(BUILD_LIBC_X86_64)
	@mkdir -p $(ISO_STAGING)
	$(call log_info,Build directories created)
endef

# Source file discovery functions
define find_c_sources
	$(shell find $(1) -name "*.c" 2>/dev/null)
endef

define find_asm_sources
	$(shell find $(1) -name "*.asm" 2>/dev/null)
endef