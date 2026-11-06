#include <stdio.h>

#include "servo_config.h"

int main(void)
{
    static const ServoPinMap expected[SERVO_COUNT] = {
        {4u,  SERVO_GPIO_PORT_B, 0u, 18u},
        {7u,  SERVO_GPIO_PORT_A, 4u, 14u},
        {9u,  SERVO_GPIO_PORT_A, 5u, 15u},
        {12u, SERVO_GPIO_PORT_A, 6u, 16u},
        {13u, SERVO_GPIO_PORT_A, 7u, 17u},
    };

    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        const ServoPinMap *actual = &SERVO_PIN_MAP[index];

        if ((actual->connector_number != expected[index].connector_number) ||
            (actual->gpio_port != expected[index].gpio_port) ||
            (actual->gpio_pin != expected[index].gpio_pin) ||
            (actual->mcu_package_pin != expected[index].mcu_package_pin)) {
            fprintf(stderr, "Servo pin mapping mismatch at channel %lu\n",
                    (unsigned long)(index + 1u));
            return 1;
        }

        if ((servo_channel_offset_us(index) != (index * SERVO_SLOT_US)) ||
            ((servo_channel_offset_us(index) + SERVO_MAX_PULSE_US) >=
             SERVO_FRAME_US)) {
            fprintf(stderr, "Servo frame slot invalid at channel %lu\n",
                    (unsigned long)(index + 1u));
            return 1;
        }
    }

    if ((servo_clamp_pulse(0u) != SERVO_MIN_PULSE_US) ||
        (servo_clamp_pulse(SERVO_NEUTRAL_PULSE_US) !=
         SERVO_NEUTRAL_PULSE_US) ||
        (servo_clamp_pulse(UINT16_MAX) != SERVO_MAX_PULSE_US)) {
        fputs("Servo pulse range clamp failed\n", stderr);
        return 1;
    }

    puts("Servo pin map and pulse range passed for all 5 channels.");
    return 0;
}
