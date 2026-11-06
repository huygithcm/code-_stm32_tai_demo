# Sơ đồ chân 5 servo

Sơ đồ được trích từ `../test/STM.kicad_sch`, MCU U3 là STM32F103C8Tx gói
LQFP-48. Mỗi đầu nối servo dùng chân 1 làm tín hiệu, chân 2 cấp nguồn
`5V_IN`, chân 3 là `Earth` (GND chung của MCU).

```text
STM32 U3 (LQFP-48)                     Đầu cắm trên mạch

PB0 (pad 18) ───────────────────────── J4-1  ── Servo 1: SIGNAL
PA4 (pad 14) ───────────────────────── J7-1  ── Servo 2: SIGNAL
PA5 (pad 15) ───────────────────────── J9-1  ── Servo 3: SIGNAL
PA6 (pad 16) ───────────────────────── J12-1 ── Servo 4: SIGNAL
PA7 (pad 17) ───────────────────────── J13-1 ── Servo 5: SIGNAL

Mỗi đầu nối J4/J7/J9/J12/J13:
  Pin 1  SIGNAL (GPIO STM32, logic 3.3 V)
  Pin 2  +5V_IN
  Pin 3  Earth / GND
```

| Servo | Đầu cắm | Chân tín hiệu | Net STM32 | Chân MCU U3 |
|---|---|---:|---|---:|
| 1 | J4 | 1 | PB0 | 18 |
| 2 | J7 | 1 | PA4 | 14 |
| 3 | J9 | 1 | PA5 | 15 |
| 4 | J12 | 1 | PA6 | 16 |
| 5 | J13 | 1 | PA7 | 17 |

## Firmware kiểm tra

Firmware dùng TIM2 để phát khung 50 Hz; cạnh lên của các kênh lệch nhau 4 ms.
Góc 0–180° được ánh xạ tuyến tính sang xung 1.0–2.0 ms. Lệnh `TEST` quét
từng servo một, bước 0.25 ms; bốn servo còn lại giữ ở giữa. Sau kiểm tra, cả
năm kênh trở về 90°.

## Cổng UART hiện dùng

Firmware nhận lệnh ở **J5 / UART_1**, USART1, 9600 8N1:

| Chân J5 | Net trên schematic | Nối với USB-UART |
|---:|---|---|
| 1 | 5V | Để hở |
| 2 | PA10 / USART1_RX | TX của adapter |
| 3 | PA9 / USART1_TX | RX của adapter |
| 4 | Earth | GND của adapter |

J8 là `UART_3` (PB11/RX, PB10/TX), hiện firmware không dùng. J3 là cổng
ST-Link SWD (PA13/SWDIO, PA14/SWCLK), không phải cổng UART.

Số chân đầu nối trong tài liệu theo netlist KiCad; khi cắm, đối chiếu ký hiệu
pin trên footprint và chiều khóa của giắc. Không cấp nguồn servo từ chân
tín hiệu.
