# FreeRTOS Kernel v10.3.1

Nguồn sao chép từ bộ STM32CubeF1 có sẵn tại:
`/home/trinh/projects/embedded_os/weekly_hw/w4/STM32CubeF1/Middlewares/Third_Party/FreeRTOS/Source`.
Các file vendor được giữ nguyên nội dung và thông báo bản quyền.

- `Source/*.c`: kernel, queue, timer, event group, stream/message buffer và co-routine (đang tắt).
- `Source/include/`: public headers; `FreeRTOSConfig_template.h` chỉ là mẫu vendor.
- `Source/portable/GCC/ARM_CM3/`: port dùng cho STM32F103C8T6 và arm-none-eabi-gcc.
- `Source/portable/MemMang/heap_4.c`: cấp phát động, hỗ trợ giải phóng và gộp block trống.

Cấu hình thực tế nằm ở `bsp/FreeRTOSConfig.h`, không sửa trực tiếp source vendor.
Makefile chỉ biên dịch một port và một heap implementation.
Không có CMSIS-RTOS wrapper hoặc port cho MCU/compiler khác trong bản sao này.

Upstream: https://github.com/FreeRTOS/FreeRTOS-Kernel
License MIT nằm trong `LICENSE.md` và đầu từng file nguồn.
