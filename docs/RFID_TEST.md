# RFID: từ test phần cứng đến đầu vào ứng dụng

Cập nhật 2026-11-15. Giữ tên `task_rfid_test.h/.c` theo yêu cầu; nội dung hiện là logic RFID dùng lại trong InputTask, kèm adapter task độc lập để kiểm tra module.

## Hành vi đã chốt

- Nhận mọi thẻ đọc được UID hợp lệ 4/7/10 byte. Chưa lọc danh sách UID theo quyết định của leader.
- Mỗi lần đưa thẻ tạo tối đa một `APP_INPUT_AUTH`, `b_authorized = true`, gửi không chờ vào `APP_QUEUE_INPUT`.
- Giữ thẻ trước đầu đọc không gửi AUTH lần nữa, kể cả quá 3 giây. Phải có ba lần đọc liên tiếp NO_CARD (chu kỳ tối thiểu 50 ms) để tái nhận thẻ; đổi UID trực tiếp khi chưa xác nhận rút thẻ cũng không mở lại quyền.
- Lỗi SPI/protocol/collision không được coi là rút thẻ. Module khởi tạo lại sau 1 giây, không busy-wait và không tự phát lại sự kiện cũ.
- Khi admission đóng, phiên thay đổi trong lúc đọc hoặc queue đầy: bỏ lần quét, tăng dropped_events. Mở khóa/queue có chỗ trống không làm thẻ đang giữ tự phát AUTH.
- Module không điều khiển LED, không đổi FSM và không tự quản lý quyền chọn tầng. DisplayTask sở hữu OLED/LED trong hệ thống.

`APP_AUTH_WINDOW_MS = 3000` nằm trong app_cfg.h. ElevatorTask phải kiểm tra Emergency, admission, hai epoch và tuổi sự kiện trước khi nhận AUTH; cấp quyền đến timestamp của sự kiện + 3 giây, không tính lại từ lúc lấy queue. Hết hạn chỉ chặn yêu cầu mới; yêu cầu đã chấp nhận vẫn được phục vụ. Emergency xóa quyền ngay.

Việc cấp quyền và xử lý nút nằm ở ElevatorTask/InputTask chưa triển khai, không được coi chỉ đọc UID thành công là thang đã có luồng điều khiển hoàn chỉnh.

## Nối dây

| RC522 | STM32F103C8T6 |
|---|---|
| SCK | PA5 |
| MISO | PA6 |
| MOSI | PA7 |
| SDA/SS | PB0 (SPI NSS, không phải SDA của I²C) |
| RST | PB1 |
| VCC/GND | 3.3 V/GND chung |

BSP_SPI1_Init phải thành công trước scheduler. Driver hỗ trợ VersionReg 0x91/0x92, một thẻ; không giải quyết chọn nhiều thẻ, không đọc block/Crypto1. Phiên bản khác trả UNSUPPORTED và lưu version để chẩn đoán. Giữ nguyên driver hal_rc522 trong lần cập nhật này.

## Khởi tạo và quyền sở hữu

Trình tự trong chương trình tích hợp (không sửa main.c sản xuất ở nhánh task):

```c
/* Sau HAL_Init và cấu hình board; trước scheduler. */
configASSERT(HAL_OK == BSP_SPI1_Init());
configASSERT(pdPASS == app_ipc_init());
configASSERT(pdPASS == task_rfid_configure());
```

Chọn duy nhất một cách:

1. **Sản xuất:** InputTask gọi `task_rfid_poll()` ở chu kỳ quét nút 10 ms, trước xử lý nút. Hàm tự giới hạn chu kỳ RFID 50 ms và retry lỗi 1 giây; không tạo task RFID riêng. Tổng số task ứng dụng vẫn là 5.
2. **Kiểm tra độc lập:** gọi `task_rfid_test_create()` trước scheduler. Adapter dùng stack/priority InputTask; không tạo InputTask hoặc một chủ RC522 thứ hai.

Configure chỉ gọi một lần. Caller đầu của poll được ghi nhận là chủ sở hữu; caller khác bị từ chối. Nếu đã tạo adapter thì chỉ adapter được poll. Driver có chờ hữu hạn trong quá trình giao tiếp: chu kỳ 50 ms không phải cam kết deadline cứng; đo ảnh hưởng lên việc quét nút khi ghép InputTask.

Admission mặc định **đóng**. Khi scheduler chạy, ElevatorTask (hoặc task điều phối giả lập trong bài test) cập nhật:

```c
task_rfid_context_t context = {
    .authorization_epoch = 1u,
    .admission_epoch = 1u,
    .b_enabled = true
};
configASSERT(pdPASS == task_rfid_set_context(&context));
```

Đây chỉ là ví dụ cho trạng thái cho phép nhận input. Bản tích hợp phải đóng admission trong INIT/HOMING/EMERGENCY/queueLocked; tăng admission_epoch khi đổi phiên đóng/mở và authorization_epoch khi đổi phiên xác thực. Không cấp quyền từ một context cố định như ví dụ trong toàn bộ chương trình.

Elevator vẫn phải kiểm tra cờ Emergency phần cứng trước khi chấp nhận bất kỳ sự kiện nào: notification/context có thể được cập nhật sau ISR dừng motor. InputTask cần dùng cùng snapshot epoch để đóng gói FLOOR; không tự đoán epoch bằng 0. Bộ nhận giả lập phải drain queue khi test; nếu không, queue 6 phần tử sẽ đầy đúng thiết kế.

## Chẩn đoán

Dùng `task_rfid_test_snapshot()` từ task khác để lấy bản sao đồng bộ, hoặc breakpoint trong `rfid_present` để xem sự kiện. Snapshot gồm:

- status, uid, version và timestamp_ticks của lần poll gần nhất;
- successful_reads: số poll đọc thành công, không phải số lần đưa thẻ;
- presentations: số lần đưa thẻ đã được tiêu thụ;
- sent_events: AUTH đã vào queue, chưa có nghĩa Elevator chấp nhận;
- dropped_events: lần quét bị admission/phiên/queue từ chối.

Không còn tín hiệu xanh/đỏ của demo cũ. UID chỉ có giá trị khi status là OK. Module không tạo thêm UART/printf hoặc queue mới.

## Kiểm tra

Đã dùng mock host tạm ngoài repo để kiểm tra: thẻ bất kỳ được nhận, giữ thẻ, rút/đưa lại, NO_CARD chập chờn, lỗi rồi hồi phục không gia hạn, queue đầy không phát lại, khóa/mở không phát lại, đổi epoch giữa lúc đọc, owner khác bị từ chối, tick rollover và UID dài sai bị báo protocol error.

Build firmware mặc định vẫn là chương trình blink và có thể loại module RFID bằng linker garbage collection; đã kiểm tra thêm liên kết ARM ép giữ các API RFID (thành công). Vẫn cần chạy kit để kiểm chứng tính năng.

Trên kit cần kiểm tra: UID 4/7/10 byte, rút thẻ/giữ thẻ, nhiều thẻ, mất reader, phiên Emergency và queue đầy; đo stack/jitter khi chạy OLED + encoder + RFID. Không tạo tests/ hoặc make check trong lần cập nhật này.

## Phần sẽ bổ sung

Whitelist UID chỉ thêm khi nhóm chốt yêu cầu; hiện bất kỳ thẻ đọc được nào cũng có thể tạo AUTH. Đây là giới hạn truy cập có chủ đích, không phải cơ chế xác thực danh tính.

Header công khai API/kiểu/hằng cần dùng; logic, handle và helper giữ private trong file C. File vẫn mang tên test để giữ đường dẫn được yêu cầu; không cần một driver RC522 thứ hai.
