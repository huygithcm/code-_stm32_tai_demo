# STM32F103C8 servo UART control

Bare-metal firmware for the board described by `../test/STM.kicad_sch`. TIM2
generates 50 Hz servo pulses on all five outputs, staggered in 4 ms slots. On
startup, all servo angles are set to 90 degrees. Each requested angle from 0
to 180 degrees maps linearly to a 1.0–2.0 ms pulse.

The active command port is **J5 / UART_1**. Connect a 3.3 V USB-UART adapter:

| J5 pin | Board signal | USB-UART connection |
|---:|---|---|
| 1 | 5 V | Leave unconnected |
| 2 | PA10 / USART1 RX | Adapter TX |
| 3 | PA9 / USART1 TX | Adapter RX |
| 4 | Earth / GND | Adapter GND |

Use 9600 baud, 8 data bits, no parity, 1 stop bit (8N1). The firmware responds
on J5 pin 3. J8 is routed to UART_3 but is not used by this firmware; J3 is the
ST-Link SWD connector and does not carry the command protocol.

## Commands

Send one ASCII command followed by CR or LF:

```text
S1=45       set servo 1 to 45 degrees
S5=120      set servo 5 to 120 degrees
ALL=90      set all five servos to 90 degrees
STATUS      report the current logical angles
TEST        sweep all servos one at a time, then center them
HELP        print the command list
```

Commands are case-insensitive. `S1` through `S5` accept integer angles from 0
to 180. `TEST` takes about 28 seconds; wait for its completion response before
sending another command.

## Build and tests

Install CMake and an `arm-none-eabi` GCC toolchain, then run:

```sh
cmake --preset stm32f103c8-debug
cmake --build --preset stm32f103c8-debug
```

The build produces `build/stm32f103c8_servo_test.elf` and
`build/stm32f103c8_servo_test.bin`. The toolchain file uses the configured
STM32Cube bundle when present, or finds `arm-none-eabi-gcc` on `PATH`.

Host-side pin and command tests:

```sh
cc -std=c11 -Wall -Wextra -Werror -Isrc \
  tests/test_servo_pinout.c -o /tmp/test_servo_pinout
/tmp/test_servo_pinout
cc -std=c11 -Wall -Wextra -Werror -Isrc \
  src/servo_command.c tests/test_servo_commands.c \
  -o /tmp/test_servo_commands
/tmp/test_servo_commands
```

GitHub Actions runs both host tests and builds the firmware on pushes and pull
requests.

## Flash

With an ST-Link and `st-flash` installed:

```sh
cmake --build --preset stm32f103c8-debug --target flash
```

Flashing replaces the existing application. `current_flash_backup.bin` stores
the 64 KiB image read from the board before the previous servo firmware was
flashed.
