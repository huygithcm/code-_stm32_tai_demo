# STM32F103C8Tx sample

Bare-metal sample for the board in `../test`. It toggles unused GPIO PA0 and
sends `STM32F103C8 sample running` on USART1 TX (PA9) at 9600 baud.

The board schematic has no MCU-controlled indicator LED: D2, D8, and D10 are
power indicators. PA0 is not routed to a board header, so its signal needs a
logic probe to observe. USART1 TX is routed to J5 pin 3; J5 pin 4 is ground.
Use a 3.3 V UART adapter and leave J5 pin 1 (5 V) disconnected.

The linker script targets 64 KiB flash and 20 KiB SRAM, matching the detected
STM32F1 medium-density device. The startup code keeps the reset clock at HSI
(8 MHz), so no external clock configuration is required.

## Build

```sh
cmake --preset stm32f103c8-debug
cmake --build --preset stm32f103c8-debug
```

The build produces `build/stm32f103c8_sample.elf` and
`build/stm32f103c8_sample.bin`.

## Flash

The `flash` CMake target writes the image at `0x08000000`:

```sh
cmake --build --preset stm32f103c8-debug --target flash
```

Flashing replaces the existing application. Before this sample was flashed,
the previous 64 KiB flash contents were saved as `current_flash_backup.bin`.
Future flashes overwrite the current application; make another backup first if
you need to preserve it.
