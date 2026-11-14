# BSP SPI1 cho RC522

## Phạm vi và kết nối

Module `bsp_spi.c/.h` theo cách tổ chức `bsp_gpio.*`, `bsp_tim.*` và tên
hàm `BSP_*` hiện có. HAL vendor giữ nguyên trong `hal/`; Makefile tự nhận
source mới. Chưa nối vào main.c, chưa thêm task hoặc driver RFID/đọc UID.

| STM32 | RC522 |
|---|---|
| PA5 | SCK |
| PA6 | MISO |
| PA7 | MOSI |
| PB0 | NSS (thường in SDA/SS trên module SPI) |
| PB1 | RST |
| 3.3 V, GND | Nguồn và mass chung |

SPI master, full-duplex, 8 bit, mode 0, MSB first, NSS bằng GPIO.
Prescaler 16: SCK 500 kHz khi PCLK2 = 8 MHz; 4.5 MHz khi PCLK2 = 72 MHz.
MFRC522 hỗ trợ SPI tới 10 Mbit/s, dữ liệu được lấy mẫu tại cạnh lên.
Nguồn: [NXP MFRC522 datasheet, mục 8.1.2](https://www.nxp.com/docs/en/data-sheet/MFRC522.pdf).

## Hợp đồng API

- `BSP_SPI1_Init()`: gọi sau HAL_Init và thiết lập clock, trước scheduler;
  kiểm tra HAL_OK trước khi dùng. Tạo mutex nội bộ. Gọi lại sau init thành công
  không tái cấu hình bus. Không gọi init từ task hoặc ISR.
- PB0/NSS và PB1/RST được đặt HIGH trước khi bật output. Init không phát xung
  reset RC522 và không chờ chip khởi động. Driver RC522 sau này phải xử lý thời
  gian sẵn sàng và SoftReset có timeout trước khi cấu hình RF hoặc đọc thẻ.
- `BSP_SPI1_Transfer(tx, rx, size)`: một frame hoàn chỉnh 1..65 byte, bao gồm
  địa chỉ và các byte dummy. Hai buffer phải khác vùng nhớ, đủ size byte.
  BSP không tự mã hóa địa chỉ thanh ghi hoặc quy tắc đọc nhiều byte của RC522.
- NSS xuống LOW sau khi lấy mutex và trở về HIGH trước khi trả mutex, kể cả
  khi HAL báo lỗi. Không tách một frame thành nhiều lần gọi Transfer.
- Chỉ gọi từ task hoặc startup một luồng; không từ ISR, critical section hay
  scheduler suspended. HAL tick phải chạy. Không tự thao tác SPI1/NSS ở module khác.
- Chờ mutex tối đa 25 ms; giao dịch HAL dùng timeout riêng 25 ms. Tổng thời gian
  còn phụ thuộc lập lịch và xử lý timeout nội bộ HAL, không phải deadline 25 ms.
- Sai đối số/ngữ cảnh/chưa init trả HAL_ERROR; chờ mutex hết hạn trả HAL_TIMEOUT;
  còn lại giữ status HAL. Chỉ sử dụng rx khi HAL_OK. Đây là HAL polling, chiếm
  CPU trong lúc truyền. Chưa thêm cơ chế tự phục hồi/reset sau lỗi bus.
- Mutex chỉ bảo vệ từng frame. Một InputTask sở hữu driver RC522 và toàn bộ
  chuỗi lệnh RFID; không cho nhiều task xen kẽ chuỗi lệnh nhiều frame.
- Khi hết heap, malloc-failed hook của project có thể dừng trước khi Init trả lỗi.

Ví dụ đọc VersionReg sau khi RC522 đã sẵn sàng (không chứng minh đọc thẻ):

```c
#include "bsp_spi.h"

/* Startup sau HAL_Init, trước scheduler: kiểm tra status. */
HAL_StatusTypeDef status = BSP_SPI1_Init();

/* Trong task hoặc startup một luồng, sau khi Init trả HAL_OK. */
const uint8_t tx[] = {0xEE, 0x00}; /* Read bit | (VersionReg 0x37 << 1). */
uint8_t rx[2];
status = BSP_SPI1_Transfer(tx, rx, sizeof(tx));
/* Chỉ đọc rx[1] nếu status == HAL_OK; rx[0] không dùng. */
```

SPI không có ACK như I2C: HAL_OK chỉ xác nhận giao dịch phía STM32 hoàn tất,
không chứng minh RC522 đã kết nối. Driver phải kiểm tra dữ liệu phản hồi.

## Kiểm tra trước khi merge

Build bằng `make -j4`; main hiện vẫn blink nên linker loại SPI nếu chưa gọi.
Trên kit: kiểm tra SCK mode 0/tần số, NSS thấp xuyên suốt frame; đọc VersionReg
ổn định; xác minh mất kết nối không bị nhận nhầm là thẻ hợp lệ; kiểm tra NSS
được nhả sau lỗi và timeout không treo hệ thống. Đo độ trễ với scheduler chạy.
Chưa thêm tests/ hoặc make check; unit test chờ coding standard và test rules.

Commit trên feature/hal-spi, push cùng tên và mở PR vào develop; người còn lại
review trước khi merge. Không ghép SPI vào nhánh I2C hoặc commit trực tiếp main.
