#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

#include <stdint.h>

enum {
    SERVO_COUNT = 5,
    SERVO_MIN_PULSE_US = 1000,
    SERVO_NEUTRAL_PULSE_US = 1500,
    SERVO_MAX_PULSE_US = 2000,
};

typedef enum {
    SERVO_GPIO_PORT_A = 0,
    SERVO_GPIO_PORT_B = 1,
} ServoGpioPort;

typedef struct {
    uint8_t connector_number;
    ServoGpioPort gpio_port;
    uint8_t gpio_pin;
    uint8_t mcu_package_pin;
} ServoPinMap;

/* Extracted from ../test/STM.kicad_sch, U3 = STM32F103C8Tx LQFP-48. */
static const ServoPinMap SERVO_PIN_MAP[SERVO_COUNT] = {
    {4u,  SERVO_GPIO_PORT_B, 0u, 18u}, /* Servo 1: J4-1  -> PB0  -> U3-18 */
    {7u,  SERVO_GPIO_PORT_A, 4u, 14u}, /* Servo 2: J7-1  -> PA4  -> U3-14 */
    {9u,  SERVO_GPIO_PORT_A, 5u, 15u}, /* Servo 3: J9-1  -> PA5  -> U3-15 */
    {12u, SERVO_GPIO_PORT_A, 6u, 16u}, /* Servo 4: J12-1 -> PA6  -> U3-16 */
    {13u, SERVO_GPIO_PORT_A, 7u, 17u}, /* Servo 5: J13-1 -> PA7  -> U3-17 */
};

static inline uint16_t servo_clamp_pulse(uint16_t pulse_us)
{
    if (pulse_us < SERVO_MIN_PULSE_US) {
        return SERVO_MIN_PULSE_US;
    }
    if (pulse_us > SERVO_MAX_PULSE_US) {
        return SERVO_MAX_PULSE_US;
    }
    return pulse_us;
}

#endif
