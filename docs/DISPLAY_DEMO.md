# DisplayTask: trạng thái thang máy trên SSD1306

Cập nhật 2026-11-16. Demo đếm 10..0 đã được thay bằng DisplayTask sản xuất.
Tên tài liệu được giữ để các liên kết cũ vẫn dùng được.

## Hiển thị và quyền sở hữu

OLED SSD1306 128×64, địa chỉ I²C 7-bit 0x3C, hiển thị sáu dòng chữ ASCII:

```text
STATE: IDLE
FLOOR: 1
DIR: STOPPED
QUEUE: 0
RFID: 2.7 S
READY: YES
```

- Hỗ trợ INIT/HOMING/IDLE/MOVING/ARRIVED/EMERGENCY.
- FLOOR: ? khi current_floor = 0 (chưa xác định/giữa tầng).
- QUEUE là số yêu cầu đang chờ, không tính currentTarget.
- RFID là thời gian còn lại tại thời điểm vẽ, làm tròn lên 0.1 giây; không
  tạo hay gia hạn quyền. Trong INIT/HOMING/EMERGENCY hiển thị 0.0 S.
- LED xanh PB14 bật khi snapshot hợp lệ/còn mới, b_ready, IDLE, chiều STOPPED,
  tầng đã biết và queue rỗng. Producer chịu trách nhiệm không đích/không khóa.
- LED đỏ PB15 nháy khi Emergency, đảo mỗi 250 ms theo thời gian RTOS.
- Không có snapshot, dữ liệu sai hoặc tuổi >= 1000 ms: NO DATA, tầng/chiều/
  queue là ?, RFID 0.0 S và LED xanh tắt. Nếu trạng thái hợp lệ cuối là
  Emergency, LED đỏ tiếp tục nháy tới khi có trạng thái mới hợp lệ.

Task chỉ hiển thị, không sửa FSM, không điều khiển motor hoặc cấp quyền RFID.
Một mình DisplayTask sở hữu OLED và hai LED. Không chạy các test GPIO/RFID
cũ có ghi LED đồng thời. LED không phải đường dừng khẩn cấp.

## Hợp đồng mailbox

Dùng APP_QUEUE_DISPLAY dài 1, đã được app_ipc_init tạo bằng sizeof của
app_display_snapshot_t. Không thêm queue hay task.

Snapshot được bổ sung timestamp_ticks. ElevatorTask phải cập nhật định kỳ
ít nhất mỗi APP_DISPLAY_PERIOD_MS (200 ms), kể cả trạng thái không đổi;
phát ngay khi chuyển trạng thái quan trọng. auth_remaining_ms được tính
ở chính timestamp_ticks đó. Mọi module phải build lại với header mới.

Ví dụ bên phát, chỉ dùng tại task điều phối hoặc harness kiểm tra:

```c
app_display_snapshot_t snapshot = {
    .timestamp_ticks = xTaskGetTickCount(),
    .state = APP_STATE_IDLE,
    .direction = APP_DIRECTION_STOPPED,
    .auth_remaining_ms = 0u,
    .current_floor = 1u,
    .waiting_count = 0u,
    .b_ready = true
};
configASSERT(pdPASS == xQueueOverwrite(
    app_ipc_queue(APP_QUEUE_DISPLAY), &snapshot));
```

Đây là dữ liệu giả lập. Khi tích hợp, không hardcode tầng 1 trước HOMING;
producer lấy dữ liệu thực từ FSM và chỉ đặt b_ready khi mọi điều kiện đúng.
Task nhận với xQueueReceive, giữ bản sao riêng và kiểm tra tuổi theo tick.
Snapshot cũ không được dùng để khẳng định thang đang sẵn sàng.

## Khởi tạo

Sau HAL_Init và cấu hình clock, trước scheduler:

```c
configASSERT(HAL_OK == BSP_I2C1_Init());
configASSERT(pdPASS == app_ipc_init());
configASSERT(pdPASS == task_display_create());
/* Tạo task điều phối/publisher và các task khác trước scheduler. */
vTaskStartScheduler();
```

Header cần dùng: stm32f1xx_hal.h, bsp_i2c.h, app_ipc.h, task_display.h, task.h.
DisplayTask tự cấu hình riêng PB14/PB15 output active-high, ban đầu tắt;
không gọi BSP_GPIO_Init để tránh tác động motor/IRQ trong test độc lập.
Trong ứng dụng, khởi tạo board trước khi tạo task; không reinit GPIO lúc chạy.

Giữ priority 1, stack khởi điểm 256 words. Chờ nguồn OLED 100 ms; kiểm tra
mailbox/LED mỗi 50 ms, làm mới OLED khoảng 200 ms sau lần vẽ thành công.
Lỗi init/vẽ: ngừng lượt vẽ, chờ tối thiểu 1000 ms rồi init lại, dùng snapshot
mới nhất; trong thời gian chờ vẫn phục vụ mailbox và LED.

Tốc độ này là mục tiêu best effort. Thời gian I²C và preemption làm tăng độ
trễ hiển thị/LED; không dùng nó làm cam kết deadline dừng motor.

## Driver và nối dây

PB6 SCL, PB7 SDA, nguồn/logic phù hợp 3.3 V, GND chung, pull-up I²C 3.3 V.
Module giả định có reset nguồn riêng và charge pump nội. Không dùng SH1106.

hal_oled_write_line(row, text) ghi một hàng 8 pixel, tối đa 21 ký tự, xóa cả
phần còn lại để không lưu chữ cũ khi chuỗi ngắn đi. Chữ hoa, số, dấu hai chấm,
chấm, gạch ngang và ?; ký tự khác hiện ?. Row ngoài 0..7, NULL hoặc chuỗi quá
21 ký tự bị từ chối trước khi giao dịch. Chuỗi phải kết thúc bằng NUL.

Mỗi gói dữ liệu tối đa 16 byte + control byte, trả mutex I²C giữa các gói để
AS5600 dùng bus. Không cấp framebuffer 1024 byte trên stack. API đếm số cũ
hal_oled_show_count được giữ cho caller chẩn đoán, không được DisplayTask gọi.
Không trộn các renderer từ nhiều task.

Lệnh SSD1306 giữ nguyên từ driver trước; tham khảo [datasheet SSD1306,
sections 8 và 10](https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf).

## Kiểm tra và giới hạn

Kiểm tra host bằng mock tạm ngoài repo: mặc định NO DATA, nhận snapshot IDLE,
đếm lùi RFID theo timestamp, LED Emergency khi OLED lỗi, hết hạn dữ liệu,
snapshot sai, retry init; driver kiểm tra giới hạn hàng/chuỗi, ký tự thay thế,
xóa đuôi chữ và trả lỗi tại từng giao dịch.

Build firmware mặc định vẫn chạy PC13 blink, không gọi DisplayTask. Kiểm tra
link giữ API display giúp phát hiện lỗi liên kết mà linker GC có thể che đi;
không thay thế nạp kit. main.c sản xuất chỉ ghép ở nhánh integration theo plan.

Cần đo trên kit: hướng OLED, chữ đọc được, LED active-high, độ trễ nháy khi
OLED mất kết nối, jitter encoder khi chung bus, stack high-water mark.
Không tạo tests/ hoặc make check trong lần cập nhật này.
