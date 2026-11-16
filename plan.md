# Kế hoạch triển khai mô hình thang máy 3 tầng STM32 và FreeRTOS

Cập nhật: 2026-11-15. Đây là kế hoạch mục tiêu và danh sách công việc; không phải tuyên bố firmware đã hoàn tất hoặc đã nghiệm thu trên kit.

## 1. Cơ sở, phạm vi và quyết định thay thế

Cơ sở tham khảo:

- Báo cáo: `/home/trinh/Downloads/Bao_cao_ky_thuat_He_thong_thang_may_3_tang_STM32_FreeRTOS.docx`.
- Quy trình: `/home/trinh/Downloads/QuyTrinh-Git-2.pdf`.
- Hợp đồng hiện có: `app/inc/app_cfg.h`, `app/inc/app_ipc.h`.
- Coding standard: BARR-C:2018 và các ngoại lệ trong `docs/CODING_STANDARD.md`.

Quyết định mới của leader được ưu tiên hơn nội dung cũ trong báo cáo/kế hoạch:

1. Công tắc PA1 đặt tại tầng 1, tác động theo vị trí cabin, không lặp theo vòng motor.
2. HOMING chủ động đưa cabin về công tắc tầng 1 và xác lập vị trí 0.
3. Emergency dừng ngay; sau tối thiểu 10 giây và khi nút đã nhả, kết thúc EMERGENCY rồi HOMING về tầng 1, nếu không còn lỗi chặn chuyển động.
4. Không bắt buộc biết vị trí tuyệt đối trước khi tìm mốc. Encoder và khả năng giám sát chuyển động vẫn phải hoạt động; lỗi cảm biến/cơ khí không được bỏ qua.
5. Hiệu chuẩn thủ công không còn là luồng khởi động thông thường. Lỗi không thể tìm mốc giữ hệ thống khóa để xử lý, không tự thử lại vô hạn.

Phạm vi là mô hình mô phỏng ba tầng: RC522, AS5600, động cơ bước/ULN2003, OLED SSD1306 128×64 địa chỉ 0x3C, nút tầng, Emergency và hai LED. Không thêm cửa, mạng, lưu vị trí qua mất nguồn hoặc tối ưu lịch ngoài FIFO.

## 2. Phân công

A là leader; B là thành viên còn lại.

| Phần việc | Chủ trì | Review/phối hợp |
|---|---|---|
| I²C dùng chung, OLED, DisplayTask | A | B kiểm tra tải bus ảnh hưởng encoder |
| SPI, RC522, chính sách đọc thẻ | A | B |
| GPIO: nút tầng, Emergency, công tắc tầng 1, hai LED | B | A |
| TIM2, driver motor, AS5600, MotorTask và chuyển động HOMING | B | A |
| InputTask: ghép nút và RFID | A (đề xuất) | B cung cấp GPIO/chống dội |
| ElevatorTask/FSM, tài nguyên IPC, tích hợp và release | A (đề xuất cho vai trò leader) | B review |
| EmergencyTask và đường dừng khẩn | B (đề xuất) | A ghép FSM/recovery |
| Kiểm tra kit, hiệu chỉnh và nghiệm thu | Cả hai | Ghi kết quả đo chung |

Phần I²C/OLED/RFID, motor/encoder và GPIO phản ánh phân công đã trao đổi. Phân công task điều phối còn lại ở trên là đề xuất để tránh bỏ sót việc, cần hai người xác nhận trước khi nhận nhánh.

Chỉ leader điều phối thay đổi clock, pin map, FreeRTOSConfig, hợp đồng dùng chung và main tích hợp. Không tự sinh lại cấu hình đè lên phần của người khác.

## 3. Hiện trạng checkout và khoảng cách tới mục tiêu

Rà soát cục bộ tại ngày cập nhật; không dùng danh sách này để kết luận PR trên GitHub đã merge.

| Thành phần | Đã thấy trong source | Việc còn lại |
|---|---|---|
| BSP/HAL, CMSIS, FreeRTOS, startup, linker, Makefile | Đã có | Kiểm chứng phần cứng và xử lý lỗi khi tích hợp |
| app_cfg.h, app_ipc.h/.c | 5 task dự kiến, 6 queue | Hoàn thiện hợp đồng recovery, công tắc và phiên input |
| Stepper, AS5600, MotorTask | Đã có code | Hiệu chỉnh cơ khí và đổi HOME sang tìm mốc tự động |
| OLED, task_display | Hiển thị snapshot/FSM/RFID, LED và xử lý dữ liệu cũ | Ghép publisher ElevatorTask, đo trên kit |
| RC522, task_rfid_test | Driver, lọc lần đưa thẻ, AUTH queue và API poll cho InputTask | Đã ghép InputTask/ElevatorTask; còn kiểm chứng kit, whitelist hoãn |
| EmergencyTask, ElevatorTask, InputTask sản xuất | Đã có và ghép trong main | Kiểm chứng kit theo checklist |
| main.c | Khởi tạo năm task, IPC và scheduler | Nghiệm thu trên kit trước release |

Các giới hạn hiện tại phải xử lý rõ:

- HOME đã chuyển sang tìm mốc tự động với nhả/tìm/xác nhận có giới hạn.
- APP_MOTOR_MECHANICS_CALIBRATED = 0; các số cơ khí chưa đo vẫn bằng 0.
  Firmware mặc định báo ERROR: CONFIG và giữ motor OFF. Không tự điền số đoán.
- Tọa độ tầng 2/3, timeout và khoảng bước lấy từ cấu hình đo; không còn ép
  mỗi tầng bằng một vòng encoder. Tốc độ hiện là khoảng bước cố định; chỉ
  nghiệm thu nếu đã đo đủ thấp để không mất bước, thêm ramp nếu cơ khí cần.
- InputTask dùng lại RFID policy chung, gửi UID/epoch; nhận mọi UID hợp lệ.
- Có code không đồng nghĩa hoàn tất checklist trên kit; không đánh dấu P1–P14
  hoàn tất chỉ vì build/mock thành công. Xem docs/INTEGRATION.md.

## 4. Cấu trúc mã và phần cứng

```text
bsp/                 Pin map, clock, IRQ, cấu hình HAL/FreeRTOS, linker
hal/inc/, hal/src/   Driver thiết bị và thư viện STM32 HAL hiện có
middlewares/        FreeRTOS kernel, port Cortex-M3, heap_4
app/inc/            API task, kiểu bản tin, app_cfg.h, app_ipc.h
app/src/            Logic task, FSM, IPC và chương trình test module
main.c              Khởi tạo và ghép hệ thống tại giai đoạn tích hợp
docs/              Hướng dẫn module, coding standard và kết quả kiểm tra
plan.md             Kế hoạch và checklist chung
```

| Chức năng | Pin/ngoại vi |
|---|---|
| Emergency / công tắc tầng 1 | PA0 / PA1 |
| Nút tầng 1, 2, 3 | PA2, PA3, PA4 |
| RC522 SPI1 | PA5 SCK, PA6 MISO, PA7 MOSI; PB0 NSS, PB1 RST |
| I²C1 OLED và AS5600 | PB6 SCL, PB7 SDA; địa chỉ 0x3C và 0x36 |
| ULN2003 IN1–IN4 | PB10–PB13; TIM2 tạo nhịp pha |
| LED xanh / đỏ | PB14 / PB15 |
| SWD | PA13 / PA14 |

Giữ clock HSI 8 MHz trong bản đầu, tick HAL/RTOS 1 ms. Chốt NO/NC, mức tác động, pull-up/pull-down của các đầu vào bằng sơ đồ thực tế trước khi chạy. Không đổi pin hoặc clock riêng trong từng feature.

Header chứa API, kiểu dữ liệu và hằng cấu hình cần công khai; C chứa logic, biến private và helper static. Không đưa mọi chi tiết nội bộ vào header. Áp dụng BARR-C cho mã nhóm viết; giữ các ngoại lệ framework/vendor đã ghi nhận.

## 5. Task và tài nguyên RTOS

Có **5 task ứng dụng sản xuất**, không tính Idle/Timer của kernel. Các task demo không phải task thứ sáu cần chạy cùng hệ thống.

| Task | Priority | Stack ban đầu (word 32 bit) | Trách nhiệm |
|---|---:|---:|---|
| EmergencyTask | 5 | 256 | Nhận thông báo khẩn, theo dõi 10 giây/nhả nút, yêu cầu recovery |
| ElevatorTask | 4 | 384 | Sở hữu FSM, xác thực, FIFO và điều phối motor |
| MotorTask | 3 khi nghỉ, 4 khi hoạt động | 384 | Điều khiển chuyển động, đọc encoder, HOMING và báo cáo |
| InputTask | 3 | 384 | Quét/chống dội nút, đọc RFID, phát sự kiện |
| DisplayTask | 1 | 256 | Đọc snapshot, cập nhật OLED và LED |

configMAX_PRIORITIES = 6, heap FreeRTOS ban đầu 12 KB, INCLUDE_vTaskPrioritySet bật. Đây là ngân sách để đo; RAM tổng của STM32F103C8T6 là 20 KB, còn phải chứa biến tĩnh và stack hệ thống. Không tạo/xóa task hoặc queue theo từng chuyến.

| Queue | Số phần tử | Kiểu và quyền sở hữu |
|---|---:|---|
| APP_QUEUE_INPUT | 6 | app_input_event_t; Input → Elevator |
| APP_QUEUE_FLOOR | 2 | uint8_t; chỉ Elevator đọc/ghi/reset |
| APP_QUEUE_MOTOR_COMMAND | 1 | app_motor_command_t; Elevator → Motor |
| APP_QUEUE_MOTOR_REPORT | 4 | app_motor_report_t; Motor → Elevator |
| APP_QUEUE_POSITION | 1 | app_position_t; Motor → Elevator, mailbox |
| APP_QUEUE_DISPLAY | 1 | app_display_snapshot_t; Elevator → Display, mailbox |

Tổng **6 queue**, trong đó 2 queue dài 1 là mailbox dùng xQueueOverwrite; không overwrite lệnh/sự kiện/báo cáo hoàn tất. Có mutex I²C dùng chung OLED/encoder, mutex SPI theo BSP hiện tại và task notification riêng cho Emergency.

Quy tắc giao tiếp phải hoàn thiện trước khi ghép task:

- Elevator là chủ duy nhất sửa FSM, currentTarget, queueLocked, phiên xác thực và FloorQueue. EmergencyTask yêu cầu chuyển trạng thái, không tự sửa các biến này.
- Mọi lệnh/báo cáo motor mang command_id; hủy/chuyển phiên làm báo cáo cũ mất hiệu lực.
- Input mang timestamp, authorization_epoch, admission_epoch. Chốt cách Elevator xuất bản phiên cho Input bằng snapshot đồng bộ; không tự đọc biến private của task khác.
- Công tắc HOMING có đường phản hồi trực tiếp cho MotorTask/driver, không chờ quét RFID hoặc queue tầng. Chốt API đọc mức, sự kiện cạnh và hành vi dừng theo chiều tìm mốc.
- Emergency dùng cờ chốt và notification; queue đầy không được chặn đường dừng. Lệnh queued trước Emergency phải bị vô hiệu trước khi có thể bật pha.
- Input queue đầy: từ chối sự kiện mới có ghi nhận trạng thái, không báo đã nhận thành công. Motor command/report không gửi được: dừng/khóa hoặc giữ báo cáo pending có giới hạn theo hợp đồng, không làm mất kết quả âm thầm.
- Các task chờ notification/queue với timeout phù hợp; tránh vòng polling bận priority cao. Không giữ mutex qua delay hoặc gửi queue blocking.

## 6. FSM và chính sách phục vụ

FSM giữ sáu trạng thái hiện có; lỗi được lưu kèm nguyên nhân, không thêm FAULT.

| Trạng thái | Hành vi và chuyển tiếp |
|---|---|
| INIT | Khởi tạo thành công, không có Emergency → HOMING |
| HOMING | Tìm và xác nhận công tắc tầng 1 → đặt zero → IDLE |
| IDLE | Sẵn sàng; yêu cầu hợp lệ → MOVING |
| MOVING | Chờ báo cáo đúng command_id → ARRIVED; lỗi → EMERGENCY |
| ARRIVED | Lấy yêu cầu FIFO tiếp theo → MOVING; hết đợt → IDLE |
| EMERGENCY | Dừng/khóa, hủy yêu cầu; đủ điều kiện recovery → HOMING |

Emergency có thể ngắt mọi trạng thái. Không nhận yêu cầu tầng trong INIT/HOMING/EMERGENCY.

Chính sách RFID và FIFO:

- Đọc thành công UID của bất kỳ thẻ nào mở quyền chọn tầng trong 3 giây; lọc whitelist được hoãn theo quyết định mới của leader. Quét mới hợp lệ tạo phiên mới; giữ thẻ trước đầu đọc không gia hạn liên tục.
- Input phát sự kiện thẻ/nút; Elevator quyết định quyền và thời hạn. Khi ghép phải định nghĩa việc rút thẻ/đưa lại và thứ tự AUTH/FLOOR.
- Bỏ tầng hiện tại khi IDLE, tầng đang là đích và tầng đã có trong hàng đợi. Khi đang đi, tầng xuất phát có thể được nhận làm yêu cầu quay lại nếu không trùng đích/hàng chờ.
- Yêu cầu đầu khi IDLE trở thành currentTarget. Hai yêu cầu chờ nằm trong FloorQueue, phục vụ FIFO.
- Khi FloorQueue đầy 2 phần tử, khóa tiếp nhận tới khi cả đợt kết thúc và về IDLE; lấy một phần tử ra không mở khóa sớm.
- Hết 3 giây chỉ chặn yêu cầu mới, không hủy các yêu cầu đã nhận. Emergency hủy tất cả yêu cầu và quyền RFID.
- Sự kiện phát sinh khi bị khóa không được chạy bù sau mở khóa.

Ví dụ: tầng 1 nhận 3 → 2 → 1 thì đi 3, 2, 1; chỉ mở khóa sau khi hoàn thành cả đợt.

## 7. Emergency, HOMING và vị trí

### 7.1. Emergency

1. Ngắt chốt emergencyLatched trước, ngừng nhịp pha và đưa IN1–IN4 OFF; không chờ queue, OLED hoặc I²C.
2. Bảo vệ thao tác kiểm tra khóa/ghi pha để không có ISR hay lệnh cũ bật motor lại sau dừng.
3. EmergencyTask đánh thức Elevator để hủy đích, FIFO, quyền RFID và phiên lệnh. OLED báo Emergency; LED đỏ nháy.
4. Đếm tối thiểu 10 giây từ lần kích hoạt mới nhất bằng thời gian RTOS, không busy-wait. Chống dội để một lần nhấn không bị tính thành nhiều lần kích hoạt.
5. Đủ thời gian, nút nhả ổn định và lỗi chặn chuyển động đã hết: kết thúc EMERGENCY, giải chốt theo thao tác chống race, vào HOMING. Trong HOMING vẫn khóa yêu cầu tầng.
6. Emergency tái kích hoạt khi đang HOMING: dừng ngay và bắt đầu lại chu kỳ chờ. Không được giải chốt đè lên sự kiện Emergency mới.

Priority 5 là cao nhất trong task ứng dụng, không thay thế ưu tiên ISR. Dự kiến EXTI Emergency NVIC 5, TIM2/limit 6; kiểm tra tương thích configMAX_SYSCALL_INTERRUPT_PRIORITY trước khi gọi FromISR.

### 7.2. HOMING về tầng 1

Mốc là vị trí cabin tại công tắc tầng 1, không phải một biến hay mốc thời gian.

- Khởi động đọc mức công tắc, không chỉ chờ cạnh ngắt.
- Nếu đang tác động: chạy lên chậm để nhả, dừng khi mức nhả ổn định, rồi chạy xuống chậm bắt lại.
- Nếu chưa tác động: chạy xuống chậm tìm công tắc.
- Khi phát hiện tác động trong chiều tìm mốc, ngừng nhịp bước ngay. Xác nhận tín hiệu ổn định và encoder đứng yên rồi đặt vị trí tích lũy = 0, current_floor = 1, b_valid = true.
- Nếu xác nhận thất bại, không tiếp tục ép xuống để tìm lại vô hạn; dừng và báo lỗi.
- Pha nhả có giới hạn ngắn riêng; pha tìm có giới hạn thời gian và quãng đường phù hợp toàn hành trình. Đo tốc độ/giới hạn trên kit, không dùng mặc định 2 giây của code cũ.
- Không nhả được, không thấy tác động, encoder lỗi hoặc không tiến triển: dừng, vô hiệu mốc, vào EMERGENCY với nguyên nhân lỗi và giữ khóa tới khi xử lý.
- Chưa biết tọa độ tuyệt đối vẫn có thể tìm mốc bằng chuyển động tương đối có giám sát. Mất khả năng đọc encoder/giám sát chuyển động thì không được chạy tìm mốc tự động.

APP_MOTOR_HOME đã được cập nhật thành lệnh chạy quy trình này khi đã kết thúc Emergency hoặc khi khởi động hợp lệ. Nó không phải lệnh MOVE về count 0 khi count chưa có mốc. Xác nhận zero là bước cuối của HOME, không phải toàn bộ thao tác HOME.

### 7.3. Cơ khí và encoder

- Ưu tiên chỉnh gá công tắc để điểm dừng sau bắt mốc là tầng 1; không thêm offset phần mềm nếu không cần.
- Công tắc tác động trước chặn cứng và có hành trình dư. Thời điểm tiếp điểm đổi mức và cabin đứng yên có thể lệch do độ trễ và cơ khí.
- Một công tắc với thao tác nhả/bắt lại giúp phát hiện kẹt mức, nhưng không chứng minh phát hiện mọi hỏng hóc. Kiểm tra cả đứt dây, sai mức và chạy sai chiều.
- AS5600 đo góc trong một vòng; phần mềm theo dõi nhiều vòng và kiểm tra nam châm/tính mới của mẫu. Giới hạn khoảng cách mẫu phải phù hợp tốc độ thực tế, gồm cả tải I²C.
- Đo tỷ số truyền, số bước trên vòng encoder, dấu chiều và tọa độ tầng 2/3. Giới hạn sai số đến tầng phải dựa trên phép đo.
- OFF cả bốn pha khi Emergency làm mất lực giữ điện. Cơ khí phải bảo đảm cabin không trôi; phần mềm dừng phát bước không tự bảo đảm đứng yên 10 giây.

## 8. Hiển thị và thông số ban đầu

| Tham số | Giá trị/hướng xử lý |
|---|---|
| Đọc nút | 10 ms; chống dội khởi điểm 30 ms |
| Quét RFID | 50 ms; giao dịch có timeout |
| Encoder | 5 ms; đo jitter và giới hạn mất mẫu |
| OLED | 200 ms; truyền thành khối nhỏ, trả mutex giữa các khối |
| Cửa sổ RFID | 3 giây |
| Chờ Emergency | Tối thiểu 10 giây và nút đã nhả |
| LED đỏ | Đảo trạng thái mỗi 250 ms khi EMERGENCY |
| I²C | 100 kHz ban đầu |
| Tốc độ, gia tốc, timeout MOVE/HOME, khoảng nhả | Phải đo trước chạy tích hợp |

DisplayTask sản xuất hiển thị tầng (hoặc chưa xác định), trạng thái, chiều, số yêu cầu chờ và thời gian quyền RFID còn lại. Snapshot đã có nguyên nhân lỗi CONFIG/SENSOR/MOTOR/IPC, đồng bộ cả bên phát và hiển thị.

LED xanh chỉ bật khi IDLE, motor dừng, không đích, FIFO rỗng và không khóa/Emergency; không đại diện RFID hợp lệ. LED đỏ nháy trong Emergency, tắt khi vận hành bình thường. DisplayTask sở hữu LED lúc chạy hệ thống; task test RFID/GPIO không được cùng ghi LED.

## 9. Các bước triển khai và nghiệm thu

Checklist nằm ngay trong plan.md để nhóm theo dõi một nơi. Các ô chưa đánh dấu không có nghĩa là chưa có code; chúng biểu thị chưa xác nhận đủ tiêu chí.

### Giai đoạn 1 — Hoàn thiện nền tảng và driver

- [ ] **P1 — Cập nhật hợp đồng HOME/recovery** — A chủ trì, B review; nhánh `feature/resources-homing`. Phạm vi app_cfg.h, app_ipc.h và tài liệu API. Phân biệt lệnh HOME với MOVE, quy định signal công tắc, phiên recovery, lỗi và timeout từng pha. Nghiệm thu: hai bên thống nhất trạng thái và không còn điều kiện bắt buộc mốc hợp lệ trước khi bắt đầu tìm mốc. Phụ thuộc: không.
- [ ] **P2 — Kiểm chứng GPIO và đường dừng khẩn** — B; `feature/emergency-stop`. Phạm vi BSP GPIO/IRQ và driver stepper. Xác nhận mức nút/công tắc, hard-stop chống race, nhả Emergency không tự bật motor. Nghiệm thu: kích hoạt khẩn khi đang phát bước không có pha mới sau dừng; queue đầy không ảnh hưởng. Phụ thuộc: P1.
- [ ] **P3 — Hiệu chỉnh motor/encoder** — B; `feature/motor-calibration`. Phạm vi cấu hình, stepper/AS5600 và tài liệu số đo. Nghiệm thu: đúng chiều, tỷ lệ, tốc độ, khoảng cách tầng, đếm vòng hai chiều và phát hiện mất mẫu. Phụ thuộc: P2.
- [ ] **P4 — Kiểm chứng I²C/OLED và SPI/RC522** — A; giữ các test riêng như `test/display`, `test/rfid`. Nghiệm thu: OLED đúng 128×64/0x3C; UID đúng; lỗi/ngắt kết nối có timeout; OLED không làm encoder mất mẫu khi chạy chung. Phụ thuộc: bus hiện có; phép đo chạy chung cần P3.

Checkpoint HAL: build thành công, các driver đã đo trên kit, ghi rõ cảnh báo còn lại và thông số cơ khí. Không coi demo chạy độc lập là đã kiểm chứng luồng hệ thống.

### Giai đoạn 2 — Task độc lập

- [ ] **P5 — HOMING trong MotorTask** — B; `feature/motor-homing`. Phạm vi task_motor và hợp đồng đã chốt. Nghiệm thu: tìm tầng 1 từ các vị trí, nhả/bắt lại khi công tắc đang tác động, giới hạn từng pha và dừng khẩn mọi pha. Phụ thuộc: P1–P3.
- [ ] **P6 — EmergencyTask** — B chủ trì đề xuất; `feature/task-emergency`. Nghiệm thu: không recovery trước 10 giây, giữ nút không chạy, nhấn mới khởi động lại thời gian, giải chốt không làm mất Emergency mới. Phụ thuộc: P1–P2. Dùng bên nhận giả lập khi chưa có Elevator.
- [ ] **P7 — InputTask** — A chủ trì đề xuất; `feature/task-input`. Nghiệm thu: chống dội/một sự kiện mỗi lần nhấn, thẻ bất kỳ có UID hợp lệ, giữ thẻ không gia hạn vô hạn, thứ tự AUTH/FLOOR và xử lý queue đầy đúng hợp đồng. Phụ thuộc: P1/P4 và GPIO. Có thể làm trước hoặc song song ElevatorTask bằng queue nhận giả lập.
- [ ] **P8 — ElevatorTask phần khởi động/recovery** — A; `feature/task-elevator`. Nghiệm thu: INIT/HOMING/IDLE, ưu tiên Emergency, bỏ báo cáo cũ, chỉ nhận thành công HOME khi mốc hợp lệ. Phụ thuộc: P1; kiểm tra giả lập trước, tích hợp sau P5/P6.
- [ ] **P9 — FIFO và quyền RFID trong ElevatorTask** — A; `feature/elevator-requests`, tách sau P8 merge. Nghiệm thu: cửa sổ 3 giây, loại trùng, hai tầng chờ, khóa cả đợt và bỏ input cũ. Phụ thuộc: P7/P8 và hợp đồng phiên.
- [ ] **P10 — DisplayTask sản xuất** — A; `feature/display-status`. Nghiệm thu: dùng snapshot thay demo đếm, tầng chưa biết không hiển thị nhầm 1, LED đúng chính sách, OLED lỗi không chặn motor/Emergency. Phụ thuộc: P1/P4; dùng snapshot giả lập, sau đó ghép P8/P9.

Mỗi task có hàm create trả trạng thái, handle private, dùng priority/stack từ app_cfg.h. Nhánh task không sửa main.c sản xuất; dùng test harness/đầu vào giả lập hoặc phiên nạp chẩn đoán riêng. Không chạy đồng thời hai chủ sở hữu driver/LED.

Checkpoint task: tạo task thành công, báo cáo lỗi đúng, đo stack và xác minh không busy-wait. Hoàn tất review trước merge develop.

### Giai đoạn 3 — Tích hợp và phát hành

- [ ] **P11 — Khởi chạy hệ thống** — A, `integration/system-v1`. Ghép board → bus/IPC → năm task → scheduler, kiểm tra mọi giá trị trả về. ISR phải xử lý được Emergency trước khi handle task sẵn sàng. Phụ thuộc: các task đã merge develop. Nghiệm thu: bật nguồn với Emergency nhấn không có chuyển động; lỗi cấp phát không bật motor.
- [ ] **P12 — Ghép theo từng luồng** — A/B. Thứ tự: khởi động/HOMING → một yêu cầu tầng → RFID/FIFO → Emergency/recovery → hiển thị đồng thời. Nghiệm thu: từng luồng vượt ma trận bên dưới, không ghép tất cả rồi mới tìm lỗi.
- [ ] **P13 — Đo tải và sửa tài liệu** — A/B. Đo heap tối thiểu, stack high-water mark, Flash/RAM, jitter encoder, độ trễ dừng; cập nhật README, pin map, hướng dẫn chạy và giới hạn driver. Nghiệm thu: không tràn stack, không mất mốc âm thầm, các thông số có số đo.
- [ ] **P14 — Release** — A. Merge integration → develop → main qua review sau nghiệm thu; tag v1.0.0. Nghiệm thu: build từ checkout sạch, nạp đúng artifact, lưu kết quả kiểm tra và hash release.

## 10. Quy trình Git

Theo tài liệu: feature tạo từ develop và PR về develop; main dành cho bản ổn định trên kit. `test/*` là quy ước bổ sung nhóm đang dùng cho kiểm tra phần cứng, không phải tên nhánh bắt buộc trong PDF.

Ví dụ bắt đầu một phần việc mới, sau khi đã xử lý rõ các thay đổi đang dở:

```sh
git status
git switch develop
git pull --ff-only origin develop
git switch -c feature/motor-homing
```

Chỉ stage file thuộc phần việc; xem diff trước commit. Ví dụ commit: `feat(task-motor): add bounded floor-one homing`. Push bằng `git push -u origin feature/motor-homing`, mở PR base develop, compare nhánh vừa làm; người còn lại review, leader merge.

- Không commit trực tiếp main. Không đổi branch trong khi chưa hiểu các thay đổi chưa commit sẽ đi theo branch mới.
- Tạo file ở branch nào không quyết định commit thuộc branch nào; branch đang checkout lúc commit mới quyết định.
- Feature đã merge, sửa thêm trên branch cũ vẫn cần commit/push và PR bổ sung; ưu tiên branch mới từ develop để tránh nhầm lịch sử.
- Kiểm tra diff của PR; hai branch giống nhau thì không có thay đổi mới để merge.
- Đồng bộ develop trước PR. Rebase/force-with-lease chỉ trên nhánh cá nhân đã thống nhất, không trên main/develop hoặc branch đang cùng dùng.
- Không commit Build/, Bin/, object, firmware output hoặc cấu hình IDE cá nhân.
- PR ghi hành vi trước/sau, file/hợp đồng thay đổi, cách kiểm tra, kết quả thực tế và phần chưa đo. Không tuyên bố đã chạy kit khi chỉ build.

## 11. Ma trận kiểm tra trước release

| Nhóm | Kịch bản và kết quả cần đạt |
|---|---|
| Startup | Bật ở tầng 1/2/3 và giữa tầng; không tự nhận tầng 1 trước xác nhận mốc |
| Công tắc active | Nhả lên có giới hạn rồi bắt lại xuống; kẹt không được zero |
| Không tìm được mốc | Đứt dây, sai mức, chạy sai chiều, motor kẹt; giới hạn dừng hữu hạn |
| Emergency | Thử ở INIT/IDLE/MOVING/HOMING và khi queue đầy; dừng không chờ task thấp |
| Recovery | Trước 10 giây không chạy; giữ nút sau 10 giây vẫn dừng; nhả hợp lệ → HOME → IDLE |
| Emergency lặp | Nhấn lại trước giải chốt hoặc đang tìm mốc; không mất sự kiện, không tự bật lại |
| Vị trí | Qua 0°/360° hai chiều, mất mẫu/nam châm, di chuyển khi mất lực giữ; mốc sai phải bị vô hiệu |
| RFID | UID đọc lỗi, hết hạn, giữ thẻ, rút/đưa lại, nhiều thẻ, mất đầu đọc; quyền đúng 3 giây |
| FIFO | 3 → 2 → 1 từ tầng 1; loại trùng; khóa không mở khi mới có một slot trống |
| Input cũ | Sự kiện trước Emergency/đổi phiên/khóa không được phục vụ sau recovery |
| Báo cáo cũ | command_id cũ không chuyển FSM hoặc xác nhận đến tầng |
| Display | OLED lỗi/ngắt kết nối không khóa dừng khẩn; LED không nhầm quyền RFID |
| Tải | OLED + RFID + encoder đồng thời; đo bus, jitter, heap, stack và lỗi queue |
| Build/release | Checkout sạch build được, đúng board/toolchain, không file sinh rác trong commit |

Không tạo lại tests/ hoặc make check trong kế hoạch này. Tiếp tục kiểm tra module/kit có ghi kết quả; framework unit test và quy tắc kiểm thử tự động được bổ sung khi nhóm chốt. Build thành công không thay thế nghiệm thu phần cứng.

## 12. Các đầu vào còn cần xác nhận

- [ ] Hai người xác nhận phân công task điều phối đề xuất ở mục 2.
- [ ] Sơ đồ NO/NC, mức tác động, vị trí gá và hành trình dư của công tắc tầng 1.
- [ ] Cơ khí không trôi khi motor OFF trong Emergency.
- [ ] Chiều quay, tỷ số truyền, tọa độ tầng, tốc độ/gia tốc, sai số dừng.
- [ ] Giới hạn nhả/tìm mốc và MOVE, dựa trên số đo toàn hành trình.
- [ ] Xác nhận phiên bản RC522 thực tế. Danh sách UID được phép sẽ bổ sung khi có yêu cầu kiểm soát truy cập.
- [ ] Ngân sách jitter encoder/độ trễ dừng và kết quả đo đủ để chọn cấu hình cuối.

Đây là các thông số chưa đo/chưa phân công rõ, không phải lý do trì hoãn việc viết task độc lập với đầu vào giả lập.

## 13. Lịch sử

- 2026-11-15: Tạo bản kế hoạch tổng hợp trong repo, thay thế bản kế hoạch cũ làm tài liệu làm việc. Giữ báo cáo/quy trình làm nguồn, cập nhật phân công và quyết định công tắc tầng 1, Emergency 10 giây rồi HOMING; phân biệt rõ code hiện có với thiết kế mục tiêu và nghiệm thu còn thiếu.

- 2026-11-15: Leader chốt tạm thời nhận mọi thẻ đọc được UID hợp lệ, mở cửa sổ nút tầng 3 giây; chưa áp dụng whitelist. Giữ thẻ không tự gia hạn.

- 2026-11-16: DisplayTask chuyển từ demo đếm sang mailbox trạng thái; thêm timestamp_ticks vào app_display_snapshot_t. Publisher phát mỗi 200 ms; snapshot quá 1000 ms hiển thị NO DATA, không báo sẵn sàng. Đã có code, chưa đánh dấu nghiệm thu P10 trên kit.

- 2026-11-16: Sửa audit và ghép integration/system-v1: năm task, HOMING có giới
  hạn, Emergency generation, report FIFO, timestamp sau I²C, RFID owner chung,
  fatal shutdown và main thực. Chưa đo cơ khí theo xác nhận của leader nên
  mặc định khóa motor; code chưa được nghiệm thu phần cứng.
