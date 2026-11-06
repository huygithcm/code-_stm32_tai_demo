# STM32F103C8 servo self-test

Bare-metal firmware for the board in `../test`. At startup it sweeps each of
the board's five servo outputs one at a time, from 1.0 ms to 2.0 ms and back in
250 us steps. Each position is held for about 500 ms; the other four outputs
stay at the 1.5 ms center pulse. After the test, all channels remain centered.

TIM2 generates a 50 Hz frame and schedules the falling edges for all five
signals. The board wiring and MCU package pad numbers are documented in
[`SERVO_PINOUT.md`](SERVO_PINOUT.md). A host-side test checks the five channel
mappings and pulse limits; it does not replace testing with connected servos.

The firmware also reports progress on USART1 TX (PA9), routed to J5 pin 3 at
9600 baud. J5 pin 4 is Earth/GND. Use a 3.3 V UART adapter and leave J5 pin 1
(5 V) disconnected.

## Build

Install CMake and an `arm-none-eabi` GCC toolchain, then run:

```sh
cmake --preset stm32f103c8-debug
cmake --build --preset stm32f103c8-debug
```

The build produces `build/stm32f103c8_servo_test.elf` and
`build/stm32f103c8_servo_test.bin`. The toolchain file uses the configured
STM32Cube bundle when present, or finds `arm-none-eabi-gcc` on `PATH`.

## Host-side checks

```sh
cc -std=c11 -Wall -Wextra -Werror -Isrc \
  tests/test_servo_pinout.c -o /tmp/test_servo_pinout
/tmp/test_servo_pinout
```

GitHub Actions runs this check and builds the firmware on pushes and pull
requests.

## Flash

With an ST-Link and `st-flash` installed, flash the test firmware with:

```sh
cmake --build --preset stm32f103c8-debug --target flash
```

Flashing replaces the existing application. `current_flash_backup.bin` is the
previous 64 KiB flash readback kept in this repository; make a new backup
before flashing if the current board application must be preserved.
