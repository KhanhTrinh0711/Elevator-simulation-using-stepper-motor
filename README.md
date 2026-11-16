# Mô hình thang máy 3 tầng — STM32F103C8T6 / FreeRTOS

Firmware C cho RC522, AS5600, motor bước/ULN2003, OLED SSD1306 128×64 0x3C,
nút tầng, Emergency và công tắc tầng 1. HSI 8 MHz, tick 1 ms; linker giới hạn
64 KB Flash và 20 KB RAM.

main.c hiện ghép năm task: Emergency, Elevator, Motor, Input, Display; dùng
sáu queue trong app_ipc.c. RFID nằm trong InputTask, không chạy task test riêng.

**Cấu hình mặc định khóa motor vì chưa đo cơ khí.** OLED báo ERROR: CONFIG.
Đọc [hướng dẫn tích hợp/hiệu chỉnh](docs/INTEGRATION.md) trước khi bật chuyển
động. Emergency dừng ngay; chỉ phục hồi sau 10 giây và nhả nút, sau đó HOMING
về công tắc tầng 1. Lỗi hệ thống giữ khóa tới khi sửa nguyên nhân và reset.

## Build

Cần make, gcc-arm-none-eabi, binutils-arm-none-eabi, libnewlib-arm-none-eabi.

```sh
make -j4       # Bin/firmware.elf, .bin, .hex; Build/firmware.map
make flash     # ST-Link: ghi vào 0x08000000 và reset
make clean     # Xóa Build/ và Bin/
```

Không có make check hoặc framework unit test trong repo. Build thành công
không chứng minh mô hình chạy đúng. Flash sẽ thay firmware đang có trên MCU.
CMakeLists.txt là mẫu IDE cũ, không phải cấu hình build firmware ARM; dùng
Makefile. Các task test module không chạy đồng thời với task sản xuất cùng
sở hữu motor, OLED, LED hay RC522.

## Tổ chức

- bsp/: board, bus, IRQ, FreeRTOSConfig, startup, linker, fatal shutdown.
- hal/inc và hal/src: driver thiết bị cùng STM32 HAL vendor.
- middlewares/FreeRTOS: kernel, Cortex-M3 port và heap_4.
- app/inc và app/src: task, FSM, IPC và cấu hình.
- [plan.md](plan.md): phân công, chính sách và nghiệm thu.
- [docs/CODING_STANDARD.md](docs/CODING_STANDARD.md): BARR-C và ngoại lệ.
- [docs/RFID_TEST.md](docs/RFID_TEST.md): RFID và phiên input.
- [docs/DISPLAY_DEMO.md](docs/DISPLAY_DEMO.md): snapshot OLED/LED.

## Nguồn thư viện

- [STM32F1 HAL](https://github.com/STMicroelectronics/stm32f1xx-hal-driver),
  v1.1.10, giấy phép tại hal/LICENSE.md.
- [CMSIS Device F1](https://github.com/STMicroelectronics/cmsis-device-f1),
  STM32F103xB/Cortex-M3; license tại bsp/CMSIS/Device/ST/STM32F1xx/License.md
  và bsp/CMSIS/LICENSE.txt.
- FreeRTOS Kernel v10.3.1: middlewares/FreeRTOS/README.md và LICENSE.md.
- Makefile phát triển từ weekly_hw/w1/Makefile. HAL/CMSIS được đưa vào repo
  từ weekly_hw/w3/bai2/Drivers; build không phụ thuộc đường dẫn đó.

Nạp qua SWDIO PA13, SWCLK PA14, GND và VTref 3.3 V; BOOT0 = 0. Không đổi
clock/pin map riêng trong từng feature. Chỉ đưa main vào bản release sau khi
đã đo và nghiệm thu trên kit.
