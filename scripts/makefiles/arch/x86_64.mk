# x86_64 (64-bit) architecture specific build rules

# Source files for x86_64 architecture
X86_64_C_SOURCES = $(call find_c_sources,$(KERNEL_DIR)/amd64)
X86_64_ASM_SOURCES = $(call find_asm_sources,$(KERNEL_DIR)/amd64)

# Object files for x86_64 (uzantı koruma: .c -> .c.o, .asm -> .asm.o)
X86_64_C_OBJECTS = $(patsubst $(KERNEL_DIR)/amd64/%.c,$(BUILD_X86_64)/%.c.o,$(X86_64_C_SOURCES))
X86_64_ASM_OBJECTS = $(patsubst $(KERNEL_DIR)/amd64/%.asm,$(BUILD_X86_64)/%.asm.o,$(X86_64_ASM_SOURCES))
X86_64_OBJECTS = $(X86_64_C_OBJECTS) $(X86_64_ASM_OBJECTS)

# Pattern rules for x86_64 C files (%.c.o)
$(BUILD_X86_64)/%.c.o: $(KERNEL_DIR)/amd64/%.c | $(BUILD_X86_64)
	@mkdir -p $(dir $@)
	$(call log_build,Compiling x86_64 specific: $<)
	@$(CC_64) $(CFLAGS_64) -c $< -o $@

# Pattern rules for x86_64 ASM files (%.asm.o)
$(BUILD_X86_64)/%.asm.o: $(KERNEL_DIR)/amd64/%.asm | $(BUILD_X86_64)
	@mkdir -p $(dir $@)
	$(call log_build,Assembling x86_64 specific: $<)
	@$(AS) $(NASMFLAGS_64) $< -o $@

# x86_64 architecture info
x86_64-info:
	@echo "=== x86_64 Architecture Information ==="
	@echo "C sources: $(X86_64_C_SOURCES)"
	@echo "ASM sources: $(X86_64_ASM_SOURCES)"
	@echo "Object files: $(X86_64_OBJECTS)"