#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>


// PS/2 Controller ports
#define PS2_DATA_PORT    0x60
#define PS2_STATUS_PORT  0x64
#define PS2_COMMAND_PORT 0x64

// PS/2 Controller commands
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_ENABLE_AUX      0xA8
#define PS2_CMD_DISABLE_AUX     0xA7
#define PS2_CMD_WRITE_TO_AUX    0xD4

// PS/2 Mouse commands
#define PS2_MOUSE_CMD_RESET             0xFF
#define PS2_MOUSE_CMD_ENABLE_REPORTING  0xF4
#define PS2_MOUSE_CMD_DISABLE_REPORTING 0xF5
#define PS2_MOUSE_CMD_SET_DEFAULTS      0xF6
#define PS2_MOUSE_CMD_GET_ID            0xF2

// PS/2 Mouse responses
#define PS2_MOUSE_ACK    0xFA
#define PS2_MOUSE_RESEND 0xFE

// Status register bits
#define PS2_STATUS_OUTPUT_FULL  0x01
#define PS2_STATUS_INPUT_FULL   0x02
#define PS2_STATUS_AUX_DATA     0x20

// Mouse packet flags
#define PS2_MOUSE_LEFT_BTN   0x01
#define PS2_MOUSE_RIGHT_BTN  0x02
#define PS2_MOUSE_MIDDLE_BTN 0x04
#define PS2_MOUSE_ALWAYS_1   0x08
#define PS2_MOUSE_X_SIGN     0x10
#define PS2_MOUSE_Y_SIGN     0x20
#define PS2_MOUSE_X_OVERFLOW 0x40
#define PS2_MOUSE_Y_OVERFLOW 0x80

// Function declarations
bool ps2mouse_init(void);
void ps2mouse_enable(void);
void ps2mouse_disable(void);
bool ps2mouse_enabled(void);
void ps2mouse_isr_handler(void);
void ps2mouse_wait_write(void);
void ps2mouse_wait_read(void);
uint8_t ps2mouse_read_data(void);
void ps2mouse_write_command(uint8_t cmd);
void ps2mouse_write_data(uint8_t data);


#ifdef __cplusplus
}
#endif