# i386 (32-bit) architecture specific build rules

# Source files for i386 architecture
I386_C_SOURCES = $(call find_c_sources,$(KERNEL_DIR)/i386)
I386_ASM_SOURCES = $(call find_asm_sources,$(KERNEL_DIR)/i386)

# Object files for i386
I386_C_OBJECTS = $(patsubst $(KERNEL_DIR)/i386/%.c,$(BUILD_X86)/%.o,$(I386_C_SOURCES))
I386_ASM_OBJECTS = $(patsubst $(KERNEL_DIR)/i386/%.asm,$(BUILD_X86)/%.o,$(I386_ASM_SOURCES))
I386_OBJECTS = $(I386_C_OBJECTS) $(I386_ASM_OBJECTS)

# Pattern rules for i386 C files
$(BUILD_X86)/%.o: $(KERNEL_DIR)/i386/%.c | $(BUILD_X86)
	@mkdir -p $(dir $@)
	$(call log_build,Compiling i386 specific: $<)
	@$(CC_32) $(CFLAGS_32) -c $< -o $@

# Pattern rules for i386 ASM files
$(BUILD_X86)/%.o: $(KERNEL_DIR)/i386/%.asm | $(BUILD_X86)
	@mkdir -p $(dir $@)
	$(call log_build,Assembling i386 specific: $<)
	@$(AS) $(NASMFLAGS_32) $< -o $@

# i386 architecture info
i386-info:
	@echo "=== i386 Architecture Information ==="
	@echo "C sources: $(I386_C_SOURCES)"
	@echo "ASM sources: $(I386_ASM_SOURCES)"
	@echo "Object files: $(I386_OBJECTS)"