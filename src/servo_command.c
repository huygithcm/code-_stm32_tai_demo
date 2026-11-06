#include "servo_command.h"

#include "servo_config.h"

static char ascii_upper(char value)
{
    if ((value >= 'a') && (value <= 'z')) {
        return (char)(value - ('a' - 'A'));
    }
    return value;
}

static const char *skip_spaces(const char *text)
{
    while ((*text == ' ') || (*text == '\t')) {
        ++text;
    }
    return text;
}

static int match_keyword(const char **text, const char *keyword)
{
    const char *cursor = *text;

    while (*keyword != '\0') {
        if (ascii_upper(*cursor) != *keyword) {
            return 0;
        }
        ++cursor;
        ++keyword;
    }

    *text = cursor;
    return 1;
}

static int line_is_complete(const char *text)
{
    return *skip_spaces(text) == '\0';
}

static int parse_angle(const char **text, uint16_t *angle_deg)
{
    const char *cursor = skip_spaces(*text);
    uint32_t value = 0u;
    int has_digit = 0;

    while ((*cursor >= '0') && (*cursor <= '9')) {
        has_digit = 1;
        value = (value * 10u) + (uint32_t)(*cursor - '0');
        if (value > 180u) {
            return 0;
        }
        ++cursor;
    }

    if (!has_digit || !line_is_complete(cursor)) {
        return 0;
    }

    *angle_deg = (uint16_t)value;
    *text = cursor;
    return 1;
}

int servo_parse_command(const char *line, ServoCommand *command)
{
    const char *cursor;

    if ((line == 0) || (command == 0)) {
        return 0;
    }

    cursor = skip_spaces(line);
    if (match_keyword(&cursor, "STATUS")) {
        if (!line_is_complete(cursor)) {
            return 0;
        }
        command->type = SERVO_COMMAND_STATUS;
        command->channel = 0u;
        command->angle_deg = 0u;
        return 1;
    }

    cursor = skip_spaces(line);
    if (match_keyword(&cursor, "HELP")) {
        if (!line_is_complete(cursor)) {
            return 0;
        }
        command->type = SERVO_COMMAND_HELP;
        command->channel = 0u;
        command->angle_deg = 0u;
        return 1;
    }

    cursor = skip_spaces(line);
    if (match_keyword(&cursor, "TEST")) {
        if (!line_is_complete(cursor)) {
            return 0;
        }
        command->type = SERVO_COMMAND_SELF_TEST;
        command->channel = 0u;
        command->angle_deg = 0u;
        return 1;
    }

    cursor = skip_spaces(line);
    if (match_keyword(&cursor, "ALL")) {
        cursor = skip_spaces(cursor);
        if (*cursor != '=') {
            return 0;
        }
        ++cursor;

        command->type = SERVO_COMMAND_SET_ALL;
        command->channel = 0u;
        return parse_angle(&cursor, &command->angle_deg);
    }

    cursor = skip_spaces(line);
    if (!match_keyword(&cursor, "S") ||
        (*cursor < '1') || (*cursor > '5')) {
        return 0;
    }

    command->channel = (uint8_t)(*cursor - '0');
    ++cursor;
    cursor = skip_spaces(cursor);
    if (*cursor != '=') {
        return 0;
    }
    ++cursor;

    command->type = SERVO_COMMAND_SET_CHANNEL;
    return parse_angle(&cursor, &command->angle_deg);
}

uint16_t servo_angle_to_pulse_us(uint16_t angle_deg)
{
    uint32_t angle = angle_deg;

    if (angle > 180u) {
        angle = 180u;
    }

    return (uint16_t)(SERVO_MIN_PULSE_US +
        ((angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) + 90u) / 180u));
}
