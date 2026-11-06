#include <stdint.h>

#include "servo_config.h"

#define RCC_APB2ENR (*(volatile uint32_t *)0x40021018u)
#define RCC_APB1ENR (*(volatile uint32_t *)0x4002101Cu)

#define GPIOA_CRL   (*(volatile uint32_t *)0x40010800u)
#define GPIOA_CRH   (*(volatile uint32_t *)0x40010804u)
#define GPIOA_BSRR  (*(volatile uint32_t *)0x40010810u)
#define GPIOB_CRL   (*(volatile uint32_t *)0x40010C00u)
#define GPIOB_BSRR  (*(volatile uint32_t *)0x40010C10u)

#define USART1_SR   (*(volatile uint32_t *)0x40013800u)
#define USART1_DR   (*(volatile uint32_t *)0x40013804u)
#define USART1_BRR  (*(volatile uint32_t *)0x40013808u)
#define USART1_CR1  (*(volatile uint32_t *)0x4001380Cu)

#define TIM2_CR1    (*(volatile uint32_t *)0x40000000u)
#define TIM2_DIER   (*(volatile uint32_t *)0x4000000Cu)
#define TIM2_SR     (*(volatile uint32_t *)0x40000010u)
#define TIM2_EGR    (*(volatile uint32_t *)0x40000014u)
#define TIM2_CCMR1  (*(volatile uint32_t *)0x40000018u)
#define TIM2_CCER   (*(volatile uint32_t *)0x40000020u)
#define TIM2_PSC    (*(volatile uint32_t *)0x40000028u)
#define TIM2_ARR    (*(volatile uint32_t *)0x4000002Cu)
#define TIM2_CCR1   (*(volatile uint32_t *)0x40000034u)

#define NVIC_ISER0  (*(volatile uint32_t *)0xE000E100u)

#define RCC_APB2_GPIOAEN (1u << 2)
#define RCC_APB2_GPIOBEN (1u << 3)
#define RCC_APB2_USART1EN (1u << 14)
#define RCC_APB1_TIM2EN (1u << 0)

#define TIM2_UIF   (1u << 0)
#define TIM2_CC1IF (1u << 1)
#define TIM2_UIE   (1u << 0)
#define TIM2_CC1IE (1u << 1)

#define SERVO_GPIOA_MASK 0xF0u
#define SERVO_GPIOB_MASK 0x01u
#define SERVO_HOLD_FRAMES 25u
#define SERVO_SWEEP_STEP_US 250u

static volatile uint16_t servo_pulse_us[SERVO_COUNT] = {
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
};
static volatile uint32_t servo_frame_count;
static volatile uint16_t servo_next_edge_us;
static volatile uint8_t servo_fallen_mask;

static void uart1_init(void)
{
    /* Reset clock is HSI at 8 MHz; USART1 is clocked from APB2. */
    RCC_APB2ENR |= RCC_APB2_GPIOAEN | RCC_APB2_USART1EN;

    /* PA9: alternate-function push-pull, 50 MHz. */
    GPIOA_CRH = (GPIOA_CRH & ~(0xFu << 4)) | (0xBu << 4);

    /* 8 MHz / 9600 baud, with the STM32F1 oversampling-by-16 divider. */
    USART1_BRR = 833u;
    USART1_CR1 = (1u << 3) | (1u << 13); /* TE | UE */
}

static void uart1_write(const char *text)
{
    while (*text != '\0') {
        while ((USART1_SR & (1u << 7)) == 0u) {
        }
        USART1_DR = (uint8_t)*text++;
    }
}

static void servo_schedule_next_edge(uint16_t after_us)
{
    uint16_t next_edge = (uint16_t)(SERVO_MAX_PULSE_US + 1u);

    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        const uint8_t bit = (uint8_t)(1u << index);
        const uint16_t pulse = servo_pulse_us[index];

        if (((servo_fallen_mask & bit) == 0u) &&
            (pulse > after_us) && (pulse < next_edge)) {
            next_edge = pulse;
        }
    }

    if (next_edge <= SERVO_MAX_PULSE_US) {
        servo_next_edge_us = next_edge;
        TIM2_CCR1 = next_edge;
        TIM2_DIER |= TIM2_CC1IE;
    } else {
        TIM2_DIER &= ~TIM2_CC1IE;
    }
}

static void servo_gpio_init(void)
{
    RCC_APB2ENR |= RCC_APB2_GPIOAEN | RCC_APB2_GPIOBEN;

    /* PA4..PA7 and PB0: 2 MHz general-purpose push-pull outputs. */
    GPIOA_CRL = (GPIOA_CRL & ~0xFFFF0000u) | 0x22220000u;
    GPIOB_CRL = (GPIOB_CRL & ~0x0000000Fu) | 0x00000002u;

    GPIOA_BSRR = SERVO_GPIOA_MASK << 16;
    GPIOB_BSRR = SERVO_GPIOB_MASK << 16;
}

static void servo_timer_init(void)
{
    RCC_APB1ENR |= RCC_APB1_TIM2EN;

    /* HSI/APB1 is 8 MHz at reset; prescale to 1 MHz (one tick per us). */
    TIM2_CR1 = 0u;
    TIM2_PSC = 7u;
    TIM2_ARR = 19999u; /* 20 ms frame: 50 Hz servo refresh. */
    TIM2_CCMR1 = 0u;
    TIM2_CCER = 0u;
    TIM2_CCR1 = SERVO_NEUTRAL_PULSE_US;
    TIM2_EGR = 1u; /* Load the prescaler immediately. */
    TIM2_SR = 0u;
    TIM2_DIER = TIM2_UIE;

    /* TIM2 is external interrupt number 28 in the STM32F103 vector table. */
    NVIC_ISER0 = 1u << 28;
    TIM2_CR1 = 1u;
}

void TIM2_IRQHandler(void)
{
    const uint32_t status = TIM2_SR;

    if ((status & TIM2_UIF) != 0u) {
        TIM2_SR = ~TIM2_UIF;
        ++servo_frame_count;
        servo_fallen_mask = 0u;

        GPIOA_BSRR = SERVO_GPIOA_MASK;
        GPIOB_BSRR = SERVO_GPIOB_MASK;

        /* Discard any stale compare flag before selecting this frame's edge. */
        TIM2_SR = ~TIM2_CC1IF;
        servo_schedule_next_edge(0u);
    }

    if ((status & TIM2_CC1IF) != 0u) {
        const uint16_t edge_us = servo_next_edge_us;
        uint32_t gpioa_reset_mask = 0u;
        uint32_t gpiob_reset_mask = 0u;

        TIM2_SR = ~TIM2_CC1IF;

        for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
            const uint8_t bit = (uint8_t)(1u << index);

            if (((servo_fallen_mask & bit) == 0u) &&
                (servo_pulse_us[index] <= edge_us)) {
                const ServoPinMap *pin = &SERVO_PIN_MAP[index];

                servo_fallen_mask |= bit;
                if (pin->gpio_port == SERVO_GPIO_PORT_A) {
                    gpioa_reset_mask |= 1u << pin->gpio_pin;
                } else {
                    gpiob_reset_mask |= 1u << pin->gpio_pin;
                }
            }
        }

        if (gpioa_reset_mask != 0u) {
            GPIOA_BSRR = gpioa_reset_mask << 16;
        }
        if (gpiob_reset_mask != 0u) {
            GPIOB_BSRR = gpiob_reset_mask << 16;
        }

        servo_schedule_next_edge(edge_us);
    }
}

static void servo_set_pulse(uint32_t index, uint16_t pulse_us)
{
    if (index < SERVO_COUNT) {
        servo_pulse_us[index] = servo_clamp_pulse(pulse_us);
    }
}

static void servo_wait_frames(uint32_t frames)
{
    const uint32_t start = servo_frame_count;

    while ((uint32_t)(servo_frame_count - start) < frames) {
        __asm volatile("wfi");
    }
}

static void servo_run_self_test(void)
{
    static const char *const servo_numbers[SERVO_COUNT] = {
        "1\r\n", "2\r\n", "3\r\n", "4\r\n", "5\r\n",
    };

    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        uart1_write("Testing SERVO ");
        uart1_write(servo_numbers[index]);

        for (uint16_t pulse = SERVO_MIN_PULSE_US;
             pulse <= SERVO_MAX_PULSE_US;
             pulse = (uint16_t)(pulse + SERVO_SWEEP_STEP_US)) {
            servo_set_pulse(index, pulse);
            servo_wait_frames(SERVO_HOLD_FRAMES);
        }

        for (int32_t pulse = SERVO_MAX_PULSE_US;
             pulse >= SERVO_MIN_PULSE_US;
             pulse -= SERVO_SWEEP_STEP_US) {
            servo_set_pulse(index, (uint16_t)pulse);
            servo_wait_frames(SERVO_HOLD_FRAMES);
        }

        servo_set_pulse(index, SERVO_NEUTRAL_PULSE_US);
        servo_wait_frames(SERVO_HOLD_FRAMES);
    }

    uart1_write("Servo self-test complete; all channels centered.\r\n");
}

int main(void)
{
    uart1_init();
    servo_gpio_init();
    servo_timer_init();

    uart1_write("STM32F103C8 servo self-test, 50 Hz\r\n");
    servo_run_self_test();

    for (;;) {
        __asm volatile("wfi");
    }
}
