# Kế hoạch Java trên Nokia 225 / MTK6260

Mục tiêu: chọn JAR/JAD từ thẻ nhớ, chạy game 2D trong VXP, phản hồi bàn phím nhanh,
giữ ngân sách 1 MiB. **Prototype đã build; tương thích game phổ biến chưa đạt.**

## Nghiên cứu

VXP chứa native ARM, JAR chứa Java classes/resources. Cần VM bytecode, CLDC class
library và MIDP APIs. [mre-makefile](https://github.com/gtrxAC/mre-makefile) cung cấp
pipeline GCC → ELF → resources → tags; upstream chỉ xác nhận MRE 3.x. Vì vậy cần
thử firmware Nokia 225 thực tế trước khi kết luận tương thích thiết bị.

[Pico JVM](https://github.com/orenskl/pico-jvm) là ứng viên port CLDC đầy đủ.
[miniJVM](https://github.com/digitalgust/miniJVM) cũng cần đánh giá RAM/thư viện/thread
khi port MRE. Lõi preview C hiện tại kiểm chứng đường JAR → bytecode → màn hình;
chưa có bằng chứng nó nhanh hơn KVM/phoneME. Hành vi tham chiếu từ
[Oracle JVM](https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-6.html) và
[MIDP Canvas](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/javax/microedition/lcdui/Canvas.html).

## Giai đoạn

| Giai đoạn | Công việc | Trạng thái / nghiệm thu |
|---|---|---|
| 1. Đường thực thi | ZIP/JAD, class loader, integer/reference, GC, MIDlet/Canvas, ARM build | Đã làm; demo và integration tests đạt |
| 2. CLDC | verifier, exceptions, long, switch/wide, strings, interfaces | Đã có tập con, monitor blocks và float cơ bản; còn verifier/interface/synchronized method và semantics đầy đủ |
| 3. Game loop | Cooperative threads, sleep/clock, Runnable, lifecycle Java, GameCanvas | Đã có cooperative workers và GameCanvas buffer/flush/key latch; lifecycle đầy đủ còn thiếu |
| 4. Đồ họa 2D | PNG bounded decode, Image/Font/clip/drawRGB, transforms, Sprite/TileLayer | PNG/clip/transform và buffer pixel oracle qua; còn Sprite/TileLayer và Graphics state riêng |
| 5. Dữ liệu/âm thanh | RMS theo suite, stream resources, MIDI/WAV qua MRE | RMS/streams và MIDI tone đã có; Player stream còn mô phỏng |
| 6. Máy thật | Game có quyền sử dụng, ma trận tương thích, RAM/FPS, profiling | Chưa làm; tiêu chí dưới đây |

## Tối ưu đã áp dụng

- Interpreter C; class theo nhu cầu; cache owner của constant-pool references.
- Giữ bytecode trong class buffer, không nhân bản mỗi instruction.
- Object handles và bounded mark/sweep; dọn VM khi đổi MIDlet.
- Một framebuffer RGB565 153.600 byte; flush sau paint hoàn tất.
- Budget 12000 opcode / khoảng 8 ms, kiểm clock mỗi 64 opcode; timer và
  repaint danh nghĩa 33 ms. GC/inflate/class loading chưa chia lát.
- VM allocations cap 816 KiB trên MRE; framebuffer thêm 150 KiB,
  code/native stack/SDK overhead ngoài tracked heap. Chưa chứng minh trên firmware.

Tối ưu tiếp theo dựa trên đo: packed byte/char arrays, ZIP file-backed thay vì giữ
cả JAR, bounded image cache, incremental inflate, method/field caching, GC pause.
JIT chưa được chọn vì cần chứng minh executable-memory/code-cache khả dụng trong MRE.

## Định nghĩa “mượt”

Theo từng game: mở được, hình/phím đúng, RAM trong 1 MiB, không tăng RAM sau 30 phút,
softkey thoát phản hồi. Mục tiêu 15 FPS: median frame interval ≤67 ms, p95 ≤100 ms,
input-to-display p95 ≤100 ms. Ghi thời gian load, GC pause, RAM đỉnh và firmware.
Chỉ công bố FPS đo trên máy thật; không suy từ tốc độ desktop/Unicorn.

Đã dùng JAR người dùng tại D:\Program\gamejava\j2me; kết quả theo tệp lưu trong
build/full-compat-20261005. Chưa kết nối Nokia 225 thật. Demo và thử nghiệm ARM
chứng minh cơ chế thực thi, chưa thay thế kiểm tra gameplay/FPS trên máy.
Phạm vi còn thiếu để đạt CLDC/MIDP đầy đủ: [coverage](CLDC-MIDP-COVERAGE.md).

## Ưu tiên sau bản ký

Giảm peak khi AoE nạp a.class/pak/PNG trong ngân sách 1 MiB. Tiếp đó kiểm tra
vào trận, save/load RMS, Graphics riêng từng đối tượng, Unicode/font và FPS máy
thật. Long/threads/exceptions/PNG/streams đã có tập con và kiểm thử bổ sung.
Không coi chữ ký hợp lệ hoặc mở JAR thành công là chứng minh gameplay.
