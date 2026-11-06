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

Firmware dùng TIM2 để phát khung 50 Hz. Từng servo được kiểm tra lần lượt
bằng xung 1.0–2.0 ms, bước 0.25 ms; bốn servo còn lại giữ ở 1.5 ms. Sau khi
kiểm tra xong, cả năm kênh ở vị trí giữa.

Số chân đầu nối trong tài liệu theo netlist KiCad; khi cắm, đối chiếu ký hiệu
pin trên footprint và chiều khóa của giắc. Không cấp nguồn servo từ chân
tín hiệu.
