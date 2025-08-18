#include <driver/ps2controller/ps2controller.h>
#include <arch.h>
#include <debug/debug.h>

#define PS2_TIMEOUT 100000

volatile bool ps2_controller_initialized = false;
volatile uint8_t ps2_controller_config = 0;

bool ps2_controller_wait_write(void) {
    int timeout = PS2_TIMEOUT;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) && timeout--) {
        io_wait();
    }
    return timeout > 0;
}

bool ps2_controller_wait_read(void) {
    int timeout = PS2_TIMEOUT;
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && timeout--) {
        io_wait();
    }
    return timeout > 0;
}

uint8_t ps2_controller_read_data(void) {
    if (!ps2_controller_wait_read()) {
        LOG("PS2 Controller: Read timeout!\n");
        return 0xFF;
    }
    return inb(PS2_DATA_PORT);
}

void ps2_controller_write_command(uint8_t cmd) {
    if (!ps2_controller_wait_write()) {
        LOG("PS2 Controller: Write command timeout!\n");
        return;
    }
    outb(PS2_COMMAND_PORT, cmd);
}

void ps2_controller_write_data(uint8_t data) {
    if (!ps2_controller_wait_write()) {
        LOG("PS2 Controller: Write data timeout!\n");
        return;
    }
    outb(PS2_DATA_PORT, data);
}

bool ps2_controller_send_command(uint8_t cmd) {
    ps2_controller_write_command(cmd);
    return true;
}

bool ps2_controller_flush_buffer(void) {
    int count = 0;
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) {
        inb(PS2_DATA_PORT);
        count++;
        if (count > 32) {
            LOG("PS2 Controller: Buffer flush overflow\n");
            return false;
        }
        io_wait();
    }
    return true;
}

uint8_t ps2_controller_get_config(void) {
    ps2_controller_write_command(PS2_CMD_READ_CONFIG);
    return ps2_controller_read_data();
}

bool ps2_controller_set_config(uint8_t config) {
    ps2_controller_write_command(PS2_CMD_WRITE_CONFIG);
    ps2_controller_write_data(config);
    // Config'i geri oku ve doğrula
    ps2_controller_config = ps2_controller_get_config();
    if (ps2_controller_config != config) {
        LOG("PS2 Controller: Config mismatch! Wrote 0x%02X, read 0x%02X\n", config, ps2_controller_config);
    }
    return true;
}

bool ps2_controller_init(void) {
    if (ps2_controller_initialized) {
        LOG("PS2 Controller: Already initialized\n");
        return true;
    }
    
    LOG("PS2 Controller: Initializing...\n");
    
    // Step 1: Disable both PS/2 ports during initialization
    ps2_controller_write_command(PS2_CMD_DISABLE_PORT1);
    ps2_controller_write_command(PS2_CMD_DISABLE_PORT2);
    
    // Step 2: Flush output buffer
    ps2_controller_flush_buffer();
    
    // Step 3: Read current configuration
    ps2_controller_config = ps2_controller_get_config();
    LOG("PS2 Controller: Initial config: 0x%02X\n", ps2_controller_config);
    
    // Step 4: Disable interrupts and translation temporarily
    uint8_t temp_config = ps2_controller_config;
    temp_config &= ~(PS2_CONFIG_PORT1_INT | PS2_CONFIG_PORT2_INT);
    temp_config &= ~PS2_CONFIG_PORT1_TRANS;  // Disable translation for raw scancodes
    ps2_controller_set_config(temp_config);
    
    // Step 5: Perform controller self-test
    ps2_controller_write_command(PS2_CMD_TEST_CONTROLLER);
    uint8_t test_result = ps2_controller_read_data();
    if (test_result != 0x55) {
        LOG("PS2 Controller: Self-test failed (0x%02X)\n", test_result);
        return false;
    }
    
    // Step 6: Restore configuration after self-test (it gets reset)
    ps2_controller_set_config(temp_config);
    
    // Step 7: Test port 1 (keyboard)
    ps2_controller_write_command(PS2_CMD_TEST_PORT1);
    test_result = ps2_controller_read_data();
    if (test_result != 0x00) {
        LOG("PS2 Controller: Port 1 test failed (0x%02X)\n", test_result);
        return false;
    }
    
    // Step 8: Enable port 1
    ps2_controller_write_command(PS2_CMD_ENABLE_PORT1);
    
    // Step 9: Check if port 2 exists (mouse port)
    // Önce port 2'yi enable et, sonra config'i oku
    ps2_controller_write_command(PS2_CMD_ENABLE_PORT2);
    ps2_controller_config = ps2_controller_get_config();
    
    // Bit 5 clear ise port 2 var demektir
    bool has_port2 = !(ps2_controller_config & PS2_CONFIG_PORT2_CLOCK);
    
    if (has_port2) {
        LOG("PS2 Controller: Port 2 (mouse) detected\n");
        
        // Test port 2
        ps2_controller_write_command(PS2_CMD_TEST_PORT2);
        test_result = ps2_controller_read_data();
        if (test_result != 0x00) {
            LOG("PS2 Controller: Port 2 test failed (0x%02X)\n", test_result);
            has_port2 = false;
        }
    } else {
        LOG("PS2 Controller: No second port detected\n");
    }
    
    // Step 10: Final configuration - DO NOT enable interrupts yet!
    // Interrupts will be enabled by individual drivers
    ps2_controller_config = ps2_controller_get_config();
    ps2_controller_config &= ~PS2_CONFIG_PORT1_TRANS;  // Disable translation
    ps2_controller_config &= ~PS2_CONFIG_PORT1_CLOCK;  // Enable keyboard clock
    ps2_controller_config &= ~PS2_CONFIG_PORT2_CLOCK;  // Enable mouse clock
    // DO NOT enable interrupts here - let drivers do it
    ps2_controller_config &= ~(PS2_CONFIG_PORT1_INT | PS2_CONFIG_PORT2_INT);
    ps2_controller_set_config(ps2_controller_config);
    
    ps2_controller_initialized = true;
    LOG("PS2 Controller: Initialization complete (config: 0x%02X)\n", ps2_controller_config);
    
    return true;
}