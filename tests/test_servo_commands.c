#include <stdio.h>

#include "servo_command.h"
#include "servo_config.h"

static int expect_command(const char *line, ServoCommandType type,
                          uint8_t channel, uint16_t angle)
{
    ServoCommand command;

    if (!servo_parse_command(line, &command)) {
        fprintf(stderr, "Command was rejected: %s\n", line);
        return 0;
    }
    if ((command.type != type) || (command.channel != channel) ||
        (command.angle_deg != angle)) {
        fprintf(stderr, "Command parsed incorrectly: %s\n", line);
        return 0;
    }
    return 1;
}

int main(void)
{
    static const uint16_t test_angles[SERVO_COUNT] = {
        0u, 45u, 90u, 135u, 180u,
    };
    static const uint16_t expected_pulses[SERVO_COUNT] = {
        1000u, 1250u, 1500u, 1750u, 2000u,
    };

    char line[16];
    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        const uint8_t channel = (uint8_t)(index + 1u);
        const int written = snprintf(line, sizeof(line), "S%u=%u",
                                     (unsigned)channel,
                                     (unsigned)test_angles[index]);
        if ((written < 0) || ((size_t)written >= sizeof(line)) ||
            !expect_command(line, SERVO_COMMAND_SET_CHANNEL, channel,
                            test_angles[index]) ||
            (servo_angle_to_pulse_us(test_angles[index]) !=
             expected_pulses[index])) {
            fprintf(stderr, "Angle mapping failed for servo %u\n",
                    (unsigned)channel);
            return 1;
        }
    }

    if (!expect_command(" ALL = 90 \t", SERVO_COMMAND_SET_ALL, 0u, 90u) ||
        !expect_command("status", SERVO_COMMAND_STATUS, 0u, 0u) ||
        !expect_command("HELP", SERVO_COMMAND_HELP, 0u, 0u) ||
        !expect_command("TEST", SERVO_COMMAND_SELF_TEST, 0u, 0u) ||
        !expect_command("s5= 180", SERVO_COMMAND_SET_CHANNEL, 5u, 180u)) {
        return 1;
    }

    static const char *const invalid_commands[] = {
        "S0=90", "S6=90", "S1=181", "S1=-1", "S1=90x",
        "ALL=999999999", "TEST NOW", "S1=", "X=90", "",
    };
    for (uint32_t index = 0u;
         index < (sizeof(invalid_commands) / sizeof(invalid_commands[0]));
         ++index) {
        ServoCommand command;
        if (servo_parse_command(invalid_commands[index], &command)) {
            fprintf(stderr, "Invalid command was accepted: %s\n",
                    invalid_commands[index]);
            return 1;
        }
    }

    if ((servo_angle_to_pulse_us(0u) != SERVO_MIN_PULSE_US) ||
        (servo_angle_to_pulse_us(90u) != SERVO_NEUTRAL_PULSE_US) ||
        (servo_angle_to_pulse_us(180u) != SERVO_MAX_PULSE_US) ||
        (servo_angle_to_pulse_us(UINT16_MAX) != SERVO_MAX_PULSE_US)) {
        fputs("Pulse conversion limits failed\n", stderr);
        return 1;
    }

    puts("UART commands and angle mapping passed for all 5 servos.");
    return 0;
}
