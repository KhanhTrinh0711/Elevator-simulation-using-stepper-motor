# Tích hợp và sửa lỗi sau audit — 2026-11-16

Nhánh làm việc: integration/system-v1, từ develop đã merge display (#16).
Đây là code tích hợp để kiểm tra, chưa phải bản đã nghiệm thu trên mô hình.

## Hành vi mặc định

main.c khởi tạo board, TIM2, I²C/SPI, sáu queue và năm task. Không tạo task
RFID/test stepper riêng. Khi chưa cấu hình cơ khí, ElevatorTask chốt lỗi CONFIG,
motor OFF, OLED hiện ERROR: CONFIG và LED đỏ nháy. Việc từ chối chuyển động
là có chủ đích, không phải lỗi build. Reset vẫn giữ trạng thái này nếu chưa
cấu hình đầy đủ. Nút nhấn/thẻ không thể vượt khóa cấu hình.

## Các sửa đổi

- Motor: đọc mức Emergency khi init/start, giữ latch đã có; kiểm tra pending
  EXTI và generation khi recovery. Emergency priority NVIC 5, TIM2/limit 6;
  thao tác cập nhật pha được bảo vệ ngắn để không bật lại pha sau dừng.
- EmergencyTask priority 5: 10 giây từ lần dừng mới nhất và nhả ổn định 30 ms.
  Generation mới hủy recovery cũ; mọi pha HOMING đều bị Emergency ngắt.
- Lỗi cấu hình/cảm biến/chuyển động/IPC chốt lỗi, không tự thử lại mỗi 10 giây.
  Phải xử lý nguyên nhân và reset. Emergency bằng nút không có lỗi nền thì
  tự chuyển qua HOMING sau thời gian chờ hợp lệ.
- HOMING: công tắc đã active thì nhả lên, đứng yên xác nhận nhả, tiếp cận
  xuống; nếu chưa active thì tiếp cận xuống. Chặn bước ngay khi PA1 active,
  xác nhận mức/encoder ổn định 50 ms rồi đặt zero. Pha nhả, tìm và xác nhận
  đều có giới hạn; sai chiều, kẹt, hết bước/thời gian hoặc mất encoder thì khóa.
- MOVE: chỉ nhận tọa độ tầng đã đo, kiểm tra bước/timeout/encoder. Quá đích
  ngoài sai số cho phép là lỗi, không giả báo đến tầng. Driver hiện dùng
  khoảng bước cố định đã đo, chưa có ramp gia tốc; chọn tốc độ phù hợp mô hình.
- Motor giữ tối đa hai báo cáo pending: một báo cáo từ chối lệnh và báo cáo
  cuối của lệnh đang chạy. Không nhận lệnh tiếp theo khi còn pending; không
  ghi đè ARRIVED/STOPPED. Emergency dừng bằng đường trực tiếp, không chờ queue.
- Timestamp encoder được lấy sau giao dịch, đánh giá khoảng mẫu bao gồm cả
  thời gian chờ bus. Tràn bộ đếm nhiều vòng trả lỗi thay vì signed overflow.
- InputTask dùng lại task_rfid_poll; một owner RC522, một context IPC chung.
  AUTH có UID và epoch. Timeout sau WUPA là lỗi đọc, không tính là rút thẻ.
- ElevatorTask sở hữu FSM, quyền 3 giây, FIFO hai tầng chờ, khóa theo đợt và
  generation/command_id. Emergency xóa yêu cầu/phiên cũ. Input trước xác thực
  hoặc mang epoch cũ bị bỏ. Giữ thẻ không gia hạn quyền.
- Display có thêm dòng ERROR và timestamp producer, hướng HOMING lấy từ
  MotorTask (nhả lên/tìm xuống/dừng). Snapshot cũ không báo READY.
- Assert, malloc/stack failure và HardFault/BusFault/MemManage/UsageFault tắt
  motor bằng register, không cần RTOS/heap, rồi dừng tại debugger.
- Task test stepper luôn nhường CPU cả khi Start thất bại.

## Cấu hình bắt buộc trước lần chạy motor

Chỉnh app/inc/app_cfg.h từ số đo, không sao chép số của mock test:

| Hằng | Ý nghĩa |
|---|---|
| APP_MOTOR_STEPS_PER_ENCODER_REV | Số nửa bước motor trên một vòng encoder |
| APP_MOTOR_POSITIVE_DIR_CW | 1: CW làm count tăng/đi lên; 0: CCW |
| APP_FLOOR_TWO_COUNTS / APP_FLOOR_THREE_COUNTS | Count tầng 2/3 so với tầng 1 |
| APP_MOTOR_STEP_INTERVAL_MS | Khoảng thời gian bước MOVE, 1..65535 ms |
| APP_HOME_STEP_INTERVAL_MS | Khoảng bước HOMING chậm, 1..65535 ms |
| APP_HOME_SEARCH_STEPS / APP_HOME_RELEASE_STEPS | Số bước tối đa tìm/nhả mốc |
| APP_HOME_SEARCH_TIMEOUT_MS / APP_HOME_RELEASE_TIMEOUT_MS | Giới hạn thời gian từng pha |
| APP_MOTOR_MOVE_TIMEOUT_MS | Giới hạn một chuyến MOVE |
| APP_MOTOR_STALL_TIMEOUT_MS | Thời gian cho phép không đổi count khi đang chạy |
| APP_MOTOR_ARRIVAL_TOLERANCE_COUNTS | Sai số đến tầng tối đa đã đo |
| APP_MOTOR_MECHANICS_CALIBRATED | Đặt 1 chỉ sau khi cấu hình/kiểm chứng các mục trên |

Count dương phải tương ứng cabin đi lên. Công tắc PA1 và Emergency PA0
active-low/pull-up; công tắc tác động trước chặn cứng. Encoder phải theo dõi
chuyển động ngay cả khi chưa biết vị trí tuyệt đối; không homing khi sensor lỗi.
Cơ khí phải không trôi khi OFF cuộn dây. Cờ calibrated không thay thế phép đo.

Tốc độ bước cố định là phạm vi hiện tại. Chỉ nghiệm thu khi tốc độ được đo là
đủ thấp để không mất bước; thêm ramp riêng nếu cơ khí cần tăng/giảm tốc.

## Build và kiểm tra

```sh
make -j4
```

Makefile dùng startup ST với -nostartfiles, giữ __libc_init_array qua hook
Newlib _init; không kéo crt0/exit/stdio teardown không dùng vào firmware.
LL utils không sử dụng bị loại khỏi source build; không sửa hoặc tắt cảnh báo
trong vendor. Không có UART/stdio backend mới được thêm.

Kiểm tra hồi quy ở /tmp/elevator-integration-check (không thêm framework,
tests/ hoặc make check vào repo): RFID, Display, Motor, FSM/FIFO, Emergency,
Stepper và khóa cấu hình mặc định. Số cơ khí trong measured.h của mock chỉ là
số tổng hợp để chạy nhánh logic, không phải thông số của board.

Các ca chính: queue report bận vẫn giữ kết quả, bus chậm làm mất tính hợp lệ,
nhả/tìm mốc/timeout, MOVE/ID cũ, FIFO 3→2→1, RFID hết hạn, Emergency khi giữ nút,
đổi generation tại recovery, startup-held PA0 và chặn PA1 theo chiều.

Chưa chạy kit: đo stack high-water mark, heap còn trống, chiều GPIO/encoder,
độ trễ dừng/LED, I²C đồng thời OLED+encoder và độ lặp lại của mốc. Không coi
build/mocks là nghiệm thu phần cứng; chưa merge main hoặc phát hành v1.0.0.
