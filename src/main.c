#include <stdint.h>

#include "servo_command.h"
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
#define USART1_RXNE (1u << 5)
#define USART1_TXE  (1u << 7)

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

#define SERVO_HOLD_FRAMES 25u
#define SERVO_SWEEP_STEP_US 250u
#define COMMAND_BUFFER_CAPACITY 32u

static volatile uint16_t servo_pulse_us[SERVO_COUNT] = {
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
    SERVO_NEUTRAL_PULSE_US,
};
static volatile uint16_t servo_angle_deg[SERVO_COUNT] = {
    90u, 90u, 90u, 90u, 90u,
};
static uint16_t servo_frame_pulse_us[SERVO_COUNT];
static volatile uint32_t servo_frame_count;
static volatile uint16_t servo_next_edge_us;
static uint8_t servo_started_mask;
static uint8_t servo_fallen_mask;

static void uart1_init(void)
{
    /* Reset clock is HSI at 8 MHz; USART1 is clocked from APB2. */
    RCC_APB2ENR |= RCC_APB2_GPIOAEN | RCC_APB2_USART1EN;

    /* PA9: AF push-pull TX; PA10: floating input RX. */
    GPIOA_CRH = (GPIOA_CRH & ~((0xFu << 4) | (0xFu << 8))) |
                (0xBu << 4) | (0x4u << 8);

    /* 8 MHz / 9600 baud, with the STM32F1 oversampling-by-16 divider. */
    USART1_BRR = 833u;
    USART1_CR1 = (1u << 2) | (1u << 3) | (1u << 13); /* RE | TE | UE */
}

static void uart1_write(const char *text)
{
    while (*text != '\0') {
        while ((USART1_SR & USART1_TXE) == 0u) {
        }
        USART1_DR = (uint8_t)*text++;
    }
}

static void uart1_write_char(char value)
{
    while ((USART1_SR & USART1_TXE) == 0u) {
    }
    USART1_DR = (uint8_t)value;
}

static void uart1_write_u16(uint16_t value)
{
    char digits[5];
    uint32_t length = 0u;

    do {
        digits[length++] = (char)('0' + (value % 10u));
        value = (uint16_t)(value / 10u);
    } while (value != 0u);

    while (length != 0u) {
        uart1_write_char(digits[--length]);
    }
}

static int uart1_read_byte(uint8_t *value)
{
    if ((USART1_SR & USART1_RXNE) == 0u) {
        return 0;
    }

    *value = (uint8_t)USART1_DR;
    return 1;
}

static void servo_gpio_write(uint32_t index, uint8_t high)
{
    const ServoPinMap *pin = &SERVO_PIN_MAP[index];
    const uint32_t bit = 1u << pin->gpio_pin;

    if (pin->gpio_port == SERVO_GPIO_PORT_A) {
        GPIOA_BSRR = high ? bit : (bit << 16);
    } else {
        GPIOB_BSRR = high ? bit : (bit << 16);
    }
}

static void servo_schedule_next_edge(uint16_t after_us)
{
    uint16_t next_edge = (uint16_t)(SERVO_FRAME_US + 1u);

    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        const uint8_t bit = (uint8_t)(1u << index);
        const uint16_t slot = servo_channel_offset_us(index);

        if ((servo_started_mask & bit) == 0u) {
            if ((slot > after_us) && (slot < next_edge)) {
                next_edge = slot;
            }
        } else if ((servo_fallen_mask & bit) == 0u) {
            const uint16_t falling_edge =
                (uint16_t)(slot + servo_frame_pulse_us[index]);
            if ((falling_edge > after_us) && (falling_edge < next_edge)) {
                next_edge = falling_edge;
            }
        }
    }

    if (next_edge < SERVO_FRAME_US) {
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

    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        servo_gpio_write(index, 0u);
    }
}

static void servo_timer_init(void)
{
    RCC_APB1ENR |= RCC_APB1_TIM2EN;

    /* HSI/APB1 is 8 MHz at reset; prescale to 1 MHz (one tick per us). */
    TIM2_CR1 = 0u;
    TIM2_PSC = 7u;
    TIM2_ARR = SERVO_FRAME_US - 1u; /* 20 ms frame: 50 Hz refresh. */
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
        for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
            servo_frame_pulse_us[index] = servo_pulse_us[index];
        }
        servo_started_mask = 0u;
        servo_fallen_mask = 0u;

        /* Stagger each channel by 4 ms to leave ample ISR time between edges. */
        servo_gpio_write(0u, 1u);
        servo_started_mask = 1u;

        /* Discard any stale compare flag before selecting this frame's edge. */
        TIM2_SR = ~TIM2_CC1IF;
        servo_schedule_next_edge(0u);
    }

    if ((status & TIM2_CC1IF) != 0u) {
        const uint16_t edge_us = servo_next_edge_us;

        TIM2_SR = ~TIM2_CC1IF;

        for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
            const uint8_t bit = (uint8_t)(1u << index);
            const uint16_t slot = servo_channel_offset_us(index);

            if (((servo_started_mask & bit) == 0u) && (slot == edge_us)) {
                servo_gpio_write(index, 1u);
                servo_started_mask |= bit;
            } else if (((servo_started_mask & bit) != 0u) &&
                       ((servo_fallen_mask & bit) == 0u) &&
                       ((uint16_t)(slot + servo_frame_pulse_us[index]) ==
                        edge_us)) {
                servo_gpio_write(index, 0u);
                servo_fallen_mask |= bit;
            }
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

static void servo_set_angle(uint32_t index, uint16_t angle_deg)
{
    if (index < SERVO_COUNT) {
        servo_angle_deg[index] = angle_deg;
        servo_set_pulse(index, servo_angle_to_pulse_us(angle_deg));
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
        servo_set_angle(index, 90u);
    }

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

        servo_set_angle(index, 90u);
        servo_wait_frames(SERVO_HOLD_FRAMES);
    }

    uart1_write("Servo self-test complete; all channels centered.\r\n");
}

static void servo_write_status(void)
{
    uart1_write("STATUS");
    for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
        uart1_write(" S");
        uart1_write_char((char)('1' + index));
        uart1_write_char('=');
        uart1_write_u16(servo_angle_deg[index]);
    }
    uart1_write("\r\n");
}

static void servo_write_help(void)
{
    uart1_write("Commands (9600 8N1):\r\n");
    uart1_write("  S1=90 .. S5=90  set one servo angle (0..180 deg)\r\n");
    uart1_write("  ALL=90          set all five angles\r\n");
    uart1_write("  STATUS          show current angles\r\n");
    uart1_write("  TEST            sweep each servo, then center all\r\n");
    uart1_write("  HELP            show commands\r\n");
}

static void servo_handle_command(const char *line)
{
    ServoCommand command;

    if (!servo_parse_command(line, &command)) {
        uart1_write("ERR invalid command; send HELP\r\n");
        return;
    }

    switch (command.type) {
    case SERVO_COMMAND_SET_CHANNEL: {
        const uint32_t index = (uint32_t)(command.channel - 1u);
        servo_set_angle(index, command.angle_deg);
        uart1_write("OK S");
        uart1_write_char((char)('0' + command.channel));
        uart1_write_char('=');
        uart1_write_u16(command.angle_deg);
        uart1_write(" deg\r\n");
        break;
    }
    case SERVO_COMMAND_SET_ALL:
        for (uint32_t index = 0u; index < SERVO_COUNT; ++index) {
            servo_set_angle(index, command.angle_deg);
        }
        uart1_write("OK ALL=");
        uart1_write_u16(command.angle_deg);
        uart1_write(" deg\r\n");
        break;
    case SERVO_COMMAND_STATUS:
        servo_write_status();
        break;
    case SERVO_COMMAND_HELP:
        servo_write_help();
        break;
    case SERVO_COMMAND_SELF_TEST:
        uart1_write("Starting servo self-test; wait for completion.\r\n");
        servo_run_self_test();
        break;
    }
}

static void uart1_process_commands(void)
{
    static char line[COMMAND_BUFFER_CAPACITY];
    static uint32_t length;
    static uint8_t overflow;
    uint8_t byte;

    if (!uart1_read_byte(&byte)) {
        return;
    }

    if ((byte == '\r') || (byte == '\n')) {
        if (overflow != 0u) {
            uart1_write("ERR command too long\r\n");
        } else if (length != 0u) {
            line[length] = '\0';
            servo_handle_command(line);
        }
        length = 0u;
        overflow = 0u;
        return;
    }

    if ((byte == 0x08u) || (byte == 0x7Fu)) {
        if ((overflow == 0u) && (length != 0u)) {
            --length;
        }
        return;
    }

    if (overflow != 0u) {
        return;
    }
    if (length < (COMMAND_BUFFER_CAPACITY - 1u)) {
        line[length++] = (char)byte;
    } else {
        overflow = 1u;
    }
}

int main(void)
{
    uart1_init();
    servo_gpio_init();
    servo_timer_init();

    uart1_write("STM32F103C8 servo UART control, 9600 8N1\r\n");
    uart1_write("All servos start at 90 deg. Send HELP for commands.\r\n");

    for (;;) {
        uart1_process_commands();
    }
}
