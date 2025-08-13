# Kernel build rules

# Include architecture-specific rules
include scripts/makefiles/arch/i386.mk
include scripts/makefiles/arch/x86_64.mk

# Source discovery
KERNEL_ARCH_INDEP_C_SOURCES = $(call find_c_sources,$(KERNEL_DIR))
KERNEL_ARCH_INDEP_ASM_SOURCES = $(call find_asm_sources,$(KERNEL_DIR))

# Filter out architecture-specific directories
KERNEL_ARCH_INDEP_C_SOURCES := $(filter-out $(KERNEL_DIR)/i386/% $(KERNEL_DIR)/amd64/%,$(KERNEL_ARCH_INDEP_C_SOURCES))
KERNEL_ARCH_INDEP_ASM_SOURCES := $(filter-out $(KERNEL_DIR)/i386/% $(KERNEL_DIR)/amd64/%,$(KERNEL_ARCH_INDEP_ASM_SOURCES))

# Object file generation for architecture-independent code
KERNEL_X86_C_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_KERNEL_X86)/%.o,$(KERNEL_ARCH_INDEP_C_SOURCES))
KERNEL_X86_ASM_OBJECTS = $(patsubst $(KERNEL_DIR)/%.asm,$(BUILD_KERNEL_X86)/%.o,$(KERNEL_ARCH_INDEP_ASM_SOURCES))

KERNEL_X86_64_C_OBJECTS = $(patsubst $(KERNEL_DIR)/%.c,$(BUILD_KERNEL_X86_64)/%.o,$(KERNEL_ARCH_INDEP_C_SOURCES))
KERNEL_X86_64_ASM_OBJECTS = $(patsubst $(KERNEL_DIR)/%.asm,$(BUILD_KERNEL_X86_64)/%.o,$(KERNEL_ARCH_INDEP_ASM_SOURCES))

.PHONY: kernel32 kernel64 verify-toolchain

# Main kernel targets
kernel32: verify-toolchain $(KERNEL_X86_ELF)
kernel64: verify-toolchain $(KERNEL_X64_ELF)

# Verify toolchain before building
verify-toolchain:
	$(verify_toolchain)

# Create build directories
$(BUILD_KERNEL_X86) $(BUILD_KERNEL_X86_64) $(BUILD_X86) $(BUILD_X86_64):
	$(call create_build_dirs)

# Pattern rules for architecture-independent C files (32-bit)
$(BUILD_KERNEL_X86)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_KERNEL_X86)
	@mkdir -p $(dir $@)
	$(call log_build,Compiling $< for 32-bit)
	@$(CC_32) $(CFLAGS_32) -c $< -o $@

# Pattern rules for architecture-independent ASM files (32-bit)
$(BUILD_KERNEL_X86)/%.o: $(KERNEL_DIR)/%.asm | $(BUILD_KERNEL_X86)
	@mkdir -p $(dir $@)
	$(call log_build,Assembling $< for 32-bit)
	@$(AS) $(NASMFLAGS_32) $< -o $@

# Pattern rules for architecture-independent C files (64-bit)
$(BUILD_KERNEL_X86_64)/%.o: $(KERNEL_DIR)/%.c | $(BUILD_KERNEL_X86_64)
	@mkdir -p $(dir $@)
	$(call log_build,Compiling $< for 64-bit)
	@$(CC_64) $(CFLAGS_64) -c $< -o $@

# Pattern rules for architecture-independent ASM files (64-bit)
$(BUILD_KERNEL_X86_64)/%.o: $(KERNEL_DIR)/%.asm | $(BUILD_KERNEL_X86_64)
	@mkdir -p $(dir $@)
	$(call log_build,Assembling $< for 64-bit)
	@$(AS) $(NASMFLAGS_64) $< -o $@

# Link 32-bit kernel
$(KERNEL_X86_ELF): $(KERNEL_X86_C_OBJECTS) $(KERNEL_X86_ASM_OBJECTS) $(I386_OBJECTS)
	$(call log_build,Linking 32-bit kernel)
	@$(LD_32) $(LDFLAGS_32) -o $@ $^
	$(call log_info,32-bit kernel built: $@)

# Link 64-bit kernel
$(KERNEL_X64_ELF): $(KERNEL_X86_64_C_OBJECTS) $(KERNEL_X86_64_ASM_OBJECTS) $(X86_64_OBJECTS)
	$(call log_build,Linking 64-bit kernel)
	@$(LD_64) $(LDFLAGS_64) -o $@ $^
	$(call log_info,64-bit kernel built: $@)

# Show kernel build info
kernel-info:
	@echo "=== Kernel Build Information ==="
	@echo "Architecture-independent C sources: $(words $(KERNEL_ARCH_INDEP_C_SOURCES))"
	@echo "Architecture-independent ASM sources: $(words $(KERNEL_ARCH_INDEP_ASM_SOURCES))"
	@echo "32-bit objects: $(words $(KERNEL_X86_C_OBJECTS) $(KERNEL_X86_ASM_OBJECTS) $(I386_OBJECTS))"
	@echo "64-bit objects: $(words $(KERNEL_X86_64_C_OBJECTS) $(KERNEL_X86_64_ASM_OBJECTS) $(X86_64_OBJECTS))"