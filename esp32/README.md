# ESP32 local Wi-Fi servo controller

This firmware makes the ESP32 its own Wi-Fi access point and serves a mobile
friendly page for setting each servo angle. It forwards commands over UART to
the existing STM32 firmware on **J5 / UART_1**. The phone/computer does not need
an internet connection.

## Hardware wiring

Use a classic ESP32 DevKit V1 with the default UART2 pins below:

| ESP32 | STM32 J5 | Signal |
|---|---|---|
| GPIO17 / UART2 TX | Pin 2 / PA10 | ESP32 TX → STM32 RX |
| GPIO16 / UART2 RX | Pin 3 / PA9 | ESP32 RX ← STM32 TX |
| GND | Pin 4 | Common ground |

Both UARTs use 3.3 V logic at 9600 baud, 8N1. Leave J5 pin 1 (5 V) unconnected.
Disconnect the USB-UART adapter from J5 while the ESP32 UART is connected, so
two transmitters do not drive the same UART lines. Power the STM32 and servos
from their normal supplies.

If the ESP32 board uses different pins, edit `SERVO_UART_RX_PIN` and
`SERVO_UART_TX_PIN` in `src/main.cpp`.

## Build and upload

Install PlatformIO Core, connect the ESP32 USB cable, then run:

```sh
cd esp32
pio run
pio run --target upload
pio device monitor
```

The serial monitor prints the access point SSID and a device-specific password.
Connect a phone or computer to that network and open:

```text
http://192.168.4.1/
```

The page queries STM32 `STATUS` on load. Each servo moves only after its
**Gửi góc** button is pressed. **Đưa cả 5 về 90°** sends `ALL=90`.

The ESP32 network is a local access point with no internet routing. Keep its
password private while the controller is connected to the servos.
