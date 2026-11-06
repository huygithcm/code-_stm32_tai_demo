#include <stdint.h>

#define RCC_APB2ENR  (*(volatile uint32_t *)0x40021018u)
#define GPIOA_CRL    (*(volatile uint32_t *)0x40010800u)
#define GPIOA_CRH    (*(volatile uint32_t *)0x40010804u)
#define GPIOA_ODR    (*(volatile uint32_t *)0x4001080Cu)
#define USART1_SR    (*(volatile uint32_t *)0x40013800u)
#define USART1_DR    (*(volatile uint32_t *)0x40013804u)
#define USART1_BRR   (*(volatile uint32_t *)0x40013808u)
#define USART1_CR1   (*(volatile uint32_t *)0x4001380Cu)

static void uart1_init(void)
{
    /* Reset clock is HSI at 8 MHz; USART1 is clocked from APB2. */
    RCC_APB2ENR |= (1u << 2) | (1u << 14); /* GPIOAEN | USART1EN */

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

static void delay(void)
{
    for (volatile uint32_t count = 0; count < 800000u; ++count) {
        __asm volatile("nop");
    }
}

int main(void)
{
    uart1_init();

    /* PA0 is unused on the board schematic, so this does not drive a servo. */
    RCC_APB2ENR |= (1u << 2); /* GPIOAEN */
    GPIOA_CRL = (GPIOA_CRL & ~0xFu) | 0x2u; /* PA0: 2 MHz push-pull output */

    for (;;) {
        GPIOA_ODR ^= 1u;
        uart1_write("STM32F103C8 sample running\r\n");
        delay();
    }
}
