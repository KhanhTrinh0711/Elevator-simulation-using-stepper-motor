# STM32F103C8T6 HAL firmware

Nền tảng lập trình C với STM32 HAL, GNU Arm Embedded và GNU Make.
Makefile được phát triển từ `/home/trinh/projects/embedded_os/weekly_hw/w1/Makefile`.
Linker dùng 64 KB Flash tại `0x08000000` và 20 KB RAM tại `0x20000000`.

## Cấu trúc project

Tổ chức theo hình ở Giai đoạn 1, Bước 1 trong `QuyTrinh-Git-2.pdf`.
Đã thêm thư viện FreeRTOS, cấu hình Cortex-M3 và nối ngắt hệ thống.
Chương trình HAL hiện tại được giữ nguyên; chưa tạo task/IPC, khởi chạy scheduler
hoặc thực hiện các bước Git trong tài liệu.

```text
Elevator-simulation-using-stepper-motor/
├── bsp/                          # Hỗ trợ MCU/board, cấu hình HAL
│   ├── FreeRTOSConfig.h
│   ├── freertos_hooks.c
│   ├── CMSIS/
│   ├── startup_stm32f103xb.s
│   ├── system_stm32f1xx.c
│   ├── stm32f1xx_it.c
│   ├── stm32f1xx_hal_conf.h
│   └── STM32F103C8.ld
├── hal/                          # Thư viện STM32 HAL hiện có
│   ├── inc/
│   ├── src/
│   └── LICENSE.md
├── middlewares/
│   └── FreeRTOS/
│       ├── LICENSE.md
│       ├── README.md
│       └── Source/               # Kernel và include/
│           └── portable/
│               ├── GCC/ARM_CM3/ # Port Cortex-M3 cho GCC
│               └── MemMang/     # Chỉ dùng heap_4.c
├── app/                          # Khung thư mục dành cho ứng dụng
│   ├── inc/
│   └── src/
├── main.c
├── Makefile
├── .gitignore
└── README.md
```

Các thư mục trống có `.gitkeep` để giữ cấu trúc khi đưa vào Git.
`main.cpp` và `CMakeLists.txt` của mẫu desktop cũ vẫn được giữ nguyên.

## Build và nạp

Cần `make`, `arm-none-eabi-gcc`, binutils ARM và Newlib (có `nano.specs`, `nosys.specs`).
Trên Debian/Ubuntu: `sudo apt install make gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi`.
`make flash` cần `st-flash` (gói `stlink-tools`) và ST-Link.

```sh
make -j4       # Tạo Bin/firmware.elf, .bin, .hex và Build/firmware.map
make flash     # Ghi firmware qua ST-Link vào 0x08000000, sau đó reset
make clean     # Xóa Build/ và Bin/
```

Chưa có target test. Unit test sẽ được bổ sung sau khi thống nhất coding standard
và unit test rules.

Kết nối ST-Link với SWDIO (PA13), SWCLK (PA14), GND và nguồn/VTref 3.3 V phù hợp board;
đặt BOOT0 = 0 để khởi động từ Flash. Lệnh flash sẽ thay chương trình đang có trên MCU.

## FreeRTOS

FreeRTOS Kernel v10.3.1 nằm trong `middlewares/FreeRTOS/Source`, dùng API FreeRTOS
trực tiếp (không thêm wrapper CMSIS-RTOS). Nguồn và giấy phép được ghi trong
`middlewares/FreeRTOS/README.md` và `LICENSE.md`.

- `bsp/FreeRTOSConfig.h`: clock theo `SystemCoreClock`, tick 1 kHz, heap 8 KB,
  5 mức ưu tiên task; bật mutex, semaphore, software timer và kiểm tra stack/heap.
- `bsp/freertos_hooks.c`: dừng tại assert khi cấp phát thất bại hoặc phát hiện tràn stack;
  dùng debugger để tìm nguyên nhân.
- `SVC_Handler` và `PendSV_Handler` được ánh xạ đến port FreeRTOS.
  `SysTick_Handler` luôn cập nhật HAL, chỉ gọi tick FreeRTOS sau khi scheduler bắt đầu,
  kể cả khi scheduler đang suspended. Giữ tick 1 kHz vì đang chia sẻ SysTick với HAL.
- IRQ gọi API `...FromISR()` phải có mức ưu tiên số từ 5 đến 15;
  ISR ưu tiên cao hơn (số 0–4) không được gọi API FreeRTOS.
- `main.c` chưa gọi `xTaskCreate()` hoặc `vTaskStartScheduler()`, nên firmware hiện tại
  vẫn chạy ví dụ nháy LED. Khi thêm task, kiểm tra kết quả tạo task và dùng
  `vTaskDelay()`/`vTaskDelayUntil()` trong task. Dung lượng heap và stack cần được
  hiệu chỉnh theo task thực tế; stack FreeRTOS tính bằng word 32 bit.


## Viết chương trình HAL

- Sửa `main.c`; Makefile tự động biên dịch các file C trong `bsp/`, `hal/src/`
  và `app/src/` (trừ template HAL).
- Header ứng dụng đặt trong `app/inc/`, header driver trong `hal/inc/`.
- Bật/tắt module trong `bsp/stm32f1xx_hal_conf.h`. Đã bật GPIO, EXTI, DMA, RCC,
  FLASH, CORTEX, PWR, TIM, UART, I2C, SPI và ADC; source HAL được build tự động theo cấu hình.
- Cấu hình chân, clock ngoại vi và NVIC/MSP theo phần cứng trước khi sử dụng ngoại vi.
- Thêm IRQ handler trong `bsp/stm32f1xx_it.c`; gọi hàm HAL IRQ handler tương ứng khi dùng ngắt.
  Giữ `SysTick_Handler` gọi `HAL_IncTick()` để timeout và `HAL_Delay()` hoạt động.

Ví dụ hiện tại nháy PC13 mỗi 500 ms, phù hợp LED tích hợp trên Blue Pill thông dụng.
Nếu board dùng chân LED khác, sửa GPIO trong `main.c`.
Clock mặc định là HSI nội 8 MHz sau reset, chưa cấu hình PLL lên 72 MHz.
Khi đổi clock, dùng HAL RCC và giá trị HSE đúng với thạch anh trên board.
Không gọi `HAL_Delay()` trong ISR ưu tiên cao hơn SysTick.

`main.cpp` và `CMakeLists.txt` là chương trình desktop mẫu ban đầu, được giữ nguyên;
chúng không tham gia build firmware. Trong CLion, dùng Makefile project để build STM32;
nút Run của cấu hình CMake desktop không nạp chương trình xuống MCU.

Newlib dùng `nosys.specs`: chưa nối `printf`/`scanf` với UART và chưa hỗ trợ file I/O.
Toolchain có thể cảnh báo `_write/_read/_close/_lseek is not implemented`;
đây là các syscall stub, không phải lỗi build. Cần retarget khi thực sự dùng I/O libc.

## Nguồn thư viện

Các file vendor được sao chép từ `weekly_hw/w3/bai2/Drivers` để project build độc lập,
không cần đường dẫn tuyệt đối hoặc tải thư viện khi build:

- [STM32F1 HAL của STMicroelectronics](https://github.com/STMicroelectronics/stm32f1xx-hal-driver):
  v1.1.10, `hal/{inc,src}`, license trong `hal/LICENSE.md`.
- [CMSIS Device F1](https://github.com/STMicroelectronics/cmsis-device-f1): header cho STM32F103xB,
  `bsp/system_stm32f1xx.c` và startup GNU `bsp/startup_stm32f103xb.s`.
  License tại `bsp/CMSIS/Device/ST/STM32F1xx/License.md`.
- CMSIS core headers trong `bsp/CMSIS/Include`, license tại `bsp/CMSIS/LICENSE.txt`.
- [Thông số STM32F103C8](https://www.st.com/en/microcontrollers-microprocessors/stm32f103c8.html).

`STM32F103xB` là macro CMSIS dùng cho dòng medium-density gồm C8;
dung lượng Flash thực tế vẫn bị linker giới hạn ở 64 KB.

Kiểm tra build không thay thế kiểm tra trên board: chưa xác nhận LED, xung clock thực tế,
ST-Link hoặc ngoại vi bằng phần cứng.

## I2C1 dùng chung cho AS5600 và OLED

`bsp/bsp_i2c.h` và `bsp/bsp_i2c.c` dùng cách đặt tên `BSP_*` như module
GPIO/TIM của nhánh `feature/hal-gpio`. Thư viện ST vẫn nằm trong `hal/`;
Makefile đã tự nhận source mới trong `bsp/`, HAL I2C đã được bật.

- I2C1: PB6/SCL, PB7/SDA, 100 kHz, alternate-function open-drain.
  Cần điện trở kéo lên 3,3 V và chung GND; kiểm tra pull-up có sẵn trên module.
- Gọi `BSP_I2C1_Init()` sau `HAL_Init()` và cấu hình clock, trước scheduler.
  Kiểm tra giá trị trả về. Hàm tạo một mutex nội bộ dùng chung cho mọi giao dịch.
- `BSP_I2C1_Write(address, data, size)` ghi gói dữ liệu, phù hợp OLED.
- `BSP_I2C1_ReadRegister(address, reg, data, size)` đọc thanh ghi địa chỉ 8 bit
  bằng repeated START, phù hợp AS5600.
- Địa chỉ đầu vào là 7 bit chưa dịch: AS5600 `0x36`, OLED thường `0x3C`
  (xác nhận module). Driver tự dịch địa chỉ theo quy ước STM32 HAL.
- Mỗi gói dài 1..32 byte; OLED nên gửi từng gói gồm control byte và tối đa
  16 byte dữ liệu, trả mutex giữa các gói để encoder có thể truy cập bus.
- Chỉ gọi từ task hoặc giai đoạn startup một luồng. Không gọi từ ISR,
  critical section hoặc khi scheduler suspended; SysTick phải đang chạy.
- Timeout chờ mutex 25 ms, timeout giao dịch HAL riêng 25 ms. Đây không phải
  cam kết deadline 25 ms cho toàn hàm: còn phụ thuộc lịch task và timeout nội bộ
  của HAL. HAL dạng polling chiếm CPU trong lúc truyền; đo lại độ trễ trên kit.
- Không lấy mutex bên ngoài hoặc gọi trực tiếp HAL I2C1 từ driver thiết bị.
  Handle và mutex được giữ private để OLED/encoder luôn đi qua cùng lớp bảo vệ.
- Lỗi đối số/ngữ cảnh/chưa init trả `HAL_ERROR`; hết thời gian chờ mutex trả
  `HAL_TIMEOUT`; lỗi giao dịch giữ nguyên status từ HAL. Không retry vô hạn.
  Nếu hết heap, malloc-failed hook hiện tại có thể dừng trước khi Init trả lỗi.

Ví dụ cho bước tích hợp sau (chưa thêm vào `main.c`):

```c
/* Startup, sau HAL_Init() và trước vTaskStartScheduler(). */
HAL_StatusTypeDef status = BSP_I2C1_Init();
/* Xử lý status trước khi cho các task dùng bus. */

/* Trong task, đọc hai byte RAW ANGLE; chỉ dùng raw khi status == HAL_OK. */
uint8_t raw[2];
status = BSP_I2C1_ReadRegister(0x36, 0x0C, raw, sizeof(raw));

/* Ví dụ gửi lệnh tắt OLED SSD1306, chưa phải driver khởi tạo màn hình. */
const uint8_t display_off[] = {0x00, 0xAE};
status = BSP_I2C1_Write(0x3C, display_off, sizeof(display_off));
```

Trước PR vào `develop`, kiểm tra trên kit: ACK của cả hai thiết bị, dạng sóng
SCL/SDA và repeated START; thiết bị không kết nối trả lỗi hữu hạn; OLED và
encoder chạy đồng thời không xen byte giữa giao dịch; bus vẫn dùng được sau
một giao dịch lỗi. Khi SDA/SCL bị giữ thấp, API trả lỗi; chưa có cơ chế tự phát
xung phục hồi bus. Không thay đổi clock/init bus trong lúc các task đang dùng.

Phạm vi nhánh này là BSP I2C và hợp đồng API; chưa thêm OLED driver,
MotorTask/encoder driver hay nối vào `main.c`. Build hiện tại vẫn chạy blink;
linker có thể loại I2C chưa được gọi. Kiểm tra compile không thay thế đo trên kit.
