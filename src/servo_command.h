#ifndef SERVO_COMMAND_H
#define SERVO_COMMAND_H

#include <stdint.h>

typedef enum {
    SERVO_COMMAND_SET_CHANNEL,
    SERVO_COMMAND_SET_ALL,
    SERVO_COMMAND_STATUS,
    SERVO_COMMAND_HELP,
    SERVO_COMMAND_SELF_TEST,
} ServoCommandType;

typedef struct {
    ServoCommandType type;
    uint8_t channel;
    uint16_t angle_deg;
} ServoCommand;

/* Parse a complete line without its CR/LF terminator. */
int servo_parse_command(const char *line, ServoCommand *command);

/* Map logical angle 0..180 degrees linearly to the configured pulse range. */
uint16_t servo_angle_to_pulse_us(uint16_t angle_deg);

#endif
