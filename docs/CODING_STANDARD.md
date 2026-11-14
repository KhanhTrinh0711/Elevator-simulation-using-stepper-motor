# BARR-C:2018 trong project

Áp dụng skill `barr-c-2018` cho C/header do nhóm viết. Giữ nguyên giao tiếp
phần cứng và API khi chuẩn hóa; thay đổi chức năng cần review riêng.

## Phạm vi đợt review

Nhánh feature/hal-i2c: main, GPIO, I2C, FreeRTOSConfig, failure hooks và SysTick.
SPI/TIM2 chưa có trong checkout này, phải review riêng trên nhánh tương ứng.
HAL/LL, CMSIS, FreeRTOS kernel/port, startup assembly, system_stm32f1xx.c và
stm32f1xx_hal_conf.h có nguồn vendor/template được miễn sửa định dạng.

## Ngoại lệ có chủ đích

- Giữ C11 trong Makefile và `_Static_assert` kiểm tra tick (BARR-C 1.1).
- Giữ API BSP_* đã thống nhất giữa các thành viên (6.1.e, 6.1.i).
- Giữ tên/kiểu HAL, FreeRTOS, main, IRQ và hook do framework yêu cầu; giữ tên
  FreeRTOSConfig.h và stm32f1xx_it.* để tương thích (4.1, 6.1, 6.5, 7.1).
- Cortex-M3 dùng ABI interrupt của startup/GCC; không tự thêm __interrupt,
  không đổi tên vector thành *_isr và không đặt handler static (6.5).
- configASSERT là macro do FreeRTOS yêu cầu, luôn dừng khi lỗi (6.3).
- Không thêm volatile cho handle I2C được mutex bảo vệ hoặc cấu hình chỉ ghi
  trước scheduler. Volatile không thay thế đồng bộ giữa task (1.8.c).
- HAL TX của phiên bản đang dùng nhận pointer không const nhưng không ghi vào
  dữ liệu. Cast bỏ const chỉ dùng tại biên gọi hàm này, có giải thích (1.6).
- Kết thúc file bằng comment và một LF, không thêm dòng trắng rỗng cuối file
  để giữ git diff --check sạch (3.3.c).
- Early return giữ luồng lỗi ngắn; quy tắc một điểm return là ưu tiên (6.2.c).

## Những gì đã sửa

- 5.3.b: dịch địa chỉ I2C trên uint32_t, tránh phép dịch trên int sau promotion.
- 5.6, 7.1: cờ dùng bool, tên biến private/handle/pointer có tiền tố.
- 1.3, 1.4, 3.x, 8.6: ngoặc riêng dòng, điều kiện rõ ràng, giới hạn 80 cột,
  hằng ở bên trái phép so sánh, vòng lặp vô hạn dùng for (;;).
- 2.2.h, 4.2, 4.3: tài liệu Doxygen, header cùng module, prototype và EOF.

Đây là chuẩn hóa source, không xác nhận hoàn chỉnh chức năng GPIO/Emergency.
PA0/PA1 vẫn chưa có EXTI; main vẫn chạy blink. Không thêm tests/ hoặc make check
trước khi nhóm chốt unit test rules. Không coi thiếu unit test là đã được kiểm tra.

## Từ viết tắt

| Từ | Nghĩa |
|---|---|
| BSP | Board Support Package |
| HAL | Hardware Abstraction Layer |
| GPIO | General Purpose Input/Output |
| I2C | Inter-Integrated Circuit |
| SPI | Serial Peripheral Interface |
| ISR / IRQ | Interrupt Service Routine / Interrupt Request |
| RTOS | Real-Time Operating System |
| OLED | Organic Light-Emitting Diode display |
| RFID | Radio-Frequency Identification |
| SCL / SDA | Serial Clock / Serial Data |
| TX / RX | Transmit / Receive |
| HSI | High-Speed Internal oscillator |
| CMSIS | Cortex Microcontroller Software Interface Standard |

## Mẫu module mới

Header `module.h`:

```c
/** @file module.h
 * @brief Purpose of the module.
 */
#ifndef MODULE_H
#define MODULE_H

/** @brief Perform the module operation. */
void module_run (void);

#endif /* MODULE_H */

/*** end of file ***/
```

Source `module.c`:

```c
/** @file module.c
 * @brief Purpose of the module.
 */
#include "module.h"

/* Order: types/constants, private data, private prototypes, public functions,
 * then private functions. Only include sections that the module needs. */

/** @brief Perform the module operation. */
void
module_run (void)
{
    /* Implement the operation here. */
}

/*** end of file ***/
```
