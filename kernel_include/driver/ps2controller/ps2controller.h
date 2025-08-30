#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// PS/2 Controller Ports
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

// PS/2 Status Register Bits
#define PS2_STATUS_OUTPUT_FULL  0x01  // Output buffer full (data available)
#define PS2_STATUS_INPUT_FULL   0x02  // Input buffer full (controller busy)
#define PS2_STATUS_SYSTEM_FLAG  0x04  // System flag
#define PS2_STATUS_COMMAND      0x08  // Command/data (0=data, 1=command)
#define PS2_STATUS_TIMEOUT_ERR  0x40  // Timeout error
#define PS2_STATUS_PARITY_ERR   0x80  // Parity error
#define PS2_STATUS_AUX_DATA     0x20  // Auxiliary data (1=mouse, 0=keyboard)

// PS/2 Controller Commands
#define PS2_CMD_READ_CONFIG     0x20  // Read controller configuration byte
#define PS2_CMD_WRITE_CONFIG    0x60  // Write controller configuration byte
#define PS2_CMD_DISABLE_PORT2   0xA7  // Disable second PS/2 port (mouse)
#define PS2_CMD_ENABLE_PORT2    0xA8  // Enable second PS/2 port (mouse)
#define PS2_CMD_TEST_PORT2      0xA9  // Test second PS/2 port
#define PS2_CMD_TEST_CONTROLLER 0xAA  // Test PS/2 controller
#define PS2_CMD_TEST_PORT1      0xAB  // Test first PS/2 port
#define PS2_CMD_DISABLE_PORT1   0xAD  // Disable first PS/2 port (keyboard)
#define PS2_CMD_ENABLE_PORT1    0xAE  // Enable first PS/2 port (keyboard)
#define PS2_CMD_WRITE_TO_AUX    0xD4  // Write next byte to auxiliary port (mouse)

// PS/2 Configuration Byte Bits
#define PS2_CONFIG_PORT1_INT    0x01  // Enable port 1 interrupt
#define PS2_CONFIG_PORT2_INT    0x02  // Enable port 2 interrupt
#define PS2_CONFIG_SYSTEM_FLAG  0x04  // System flag
#define PS2_CONFIG_PORT1_CLOCK  0x10  // Disable port 1 clock
#define PS2_CONFIG_PORT2_CLOCK  0x20  // Disable port 2 clock
#define PS2_CONFIG_PORT1_TRANS  0x40  // Enable port 1 translation

// Common PS/2 Response Codes
#define PS2_RESPONSE_ACK        0xFA  // Acknowledge
#define PS2_RESPONSE_RESEND     0xFE  // Resend last byte
#define PS2_RESPONSE_ERROR      0xFC  // Error
#define PS2_RESPONSE_TEST_OK    0xAA  // Self-test passed

// Function declarations for shared PS/2 controller operations
bool ps2_controller_init(void);
bool ps2_controller_wait_write(void);
bool ps2_controller_wait_read(void);
uint8_t ps2_controller_read_data(void);
void ps2_controller_write_command(uint8_t cmd);
void ps2_controller_write_data(uint8_t data);
bool ps2_controller_send_command(uint8_t cmd);
uint8_t ps2_controller_get_config(void);
bool ps2_controller_set_config(uint8_t config);
bool ps2_controller_flush_buffer(void);

// Synchronization for controller access
extern volatile bool ps2_controller_initialized;
extern volatile uint8_t ps2_controller_config;

#ifdef __cplusplus
}
#endif