# Sửa lỗi mở và chạy ARENA.jar

## Triệu chứng ban đầu

Launcher báo `Cannot read ZIP directory` khi mở `ARENA.jar` trên VXPEmu.
Sau khi sửa lớp đọc tệp MRE, lỗi kế tiếp là
`Invalid constant pool reference` tại `VNAMidlet.startApp` (opcode `new Intro`).

## Các sửa đổi trong lõi

### 1. Đọc tệp MRE

Đã có từ bản trước (xem [MRE-FILE-IO-FIX.md](MRE-FILE-IO-FIX.md)):
`vm_file_seek` dùng `BASE_BEGIN`, kiểm tra `vm_file_getfilesize(...) != 0`,
và `read_exact` đọc lặp từng khối 1 KiB.

### 2. `ConstantValue` cho hằng `String`

`native/vm.c` chỉ chấp nhận `ConstantValue` trỏ tới `CONSTANT_Integer` (tag 3).
`ARENA.jar` có nhiều `static final String` (ví dụ `Intro.TITLE`, `Light`), nên
bộ nạp lớp báo `Invalid constant pool reference` ngay khi nạp `Intro`.

Sửa:

- `Field` thêm `constant` để giữ chỉ số CP của hằng `String`.
- `attrs()` chấp nhận tag 3/4/5/6 và tag 8; tag 8 được hoãn lại.
- `initialize()` tạo đối tượng `String` từ literal trước khi chạy `<clinit>`.

### 3. API Nokia/MIDP còn thiếu

Thêm vào bảng native:

- `javax/microedition/lcdui/Display.flashBacklight(I)V` (no-op)
- `Display.vibrate(I)Z`
- `Canvas.getKeyName(I)Ljava/lang/String;`
- lớp builtin `com/nokia/mid/ui/DeviceControl` với `setLights`,
  `flashLights`, `vibrate`, `startVibra`, `stopVibra`, `getLights`

### 4. Cấp phát object sau GC

`allocobj()` chỉ thử 2 lượt; nếu bảng object hết slot và lần cấp page mới thất bại
trước khi GC kịp giải phóng, lượt thử sau không được chạy lại. Tăng lên 3 lượt
để cấp phát lại sau GC.

### 5. Ngân sách RAM

ARENA cần khoảng 1,18 MiB heap đỉnh (đo trên host). Nâng yêu cầu RAM của VXP
từ 1024 KiB lên **2048 KiB** và giới hạn heap MRE từ 816 KiB lên **1536 KiB**:

- `tools/build.py`: `required_ram=2048`
- `project.json`: `ram_kb: 2048`
- `native-project.json`: `required_ram_kib: 2048`
- `native/pocket.h`: `PJ_MEMORY_LIMIT (1536u*1024u)` cho `MRE`

Host 64-bit vẫn giữ 896 KiB để test tiết kiệm bộ nhớ.

## Kiểm chứng

- Host: `build/pocketjava.exe ARENA.jar` chạy qua màn hình giới thiệu, splash
  và tới menu đăng nhập (Khách / Garena / Facebook).
- VXPEmu: mở `ARENA.jar` hiện màn hình `LỜI NÓI ĐẦU`, sau đó là splash
  `LIÊN QUÂN MOBILE`.
- `tests/test_runtime.py`: 31 nhóm tích hợp, 120 ca mutation — pass.
- `tools/arm_smoke.py`: pass, heap sau quit = 0.

## Chưa xác nhận

- Chơi hết một trận hoặc vào gameplay thực tế.
- Firmware Nokia 225 thật và mức RAM 2 MiB.
- Các API mạng/thanh toán của ARENA (nếu có) vẫn chưa được hỗ trợ.
