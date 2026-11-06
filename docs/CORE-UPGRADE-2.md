# Nâng cấp lõi tiếp theo — 05/10/2026

Đợt này xử lý API chặn Moto Racer Fury, Mã Đao 2 và Superman trong báo cáo 42 JAR trước đó.
JAR nguồn không sửa; game vẫn dùng class/bytecode và tài nguyên thật.

- StringBuffer.delete/deleteCharAt: chỉ số ký tự BMP, clamp end, kiểm tra bounds và thu nhỏ buffer.
- StringBuffer.setLength: cắt chuỗi hoặc thêm ký tự NUL bằng biểu diễn nội bộ modified UTF-8, giới hạn kích thước/RAM.
- StringBuffer.append(long): định dạng đúng cả Long.MIN_VALUE/MAX_VALUE, scratch trên stack.
- String.valueOf(char): ký tự BMP và NUL giữ đúng độ dài một ký tự.
- Vector constructor ()/(I)/(II), capacity, tăng capacity theo increment hoặc doubling; không cấp phát cố định 16 phần tử cho mọi yêu cầu.
- ByteArrayInputStream constructor offset/count, clamp count, mark/reset. Close không làm mất khả năng đọc dữ liệu byte array.
- DataInputStream ủy quyền mark/reset tới stream bên dưới, có giới hạn độ sâu wrapper; resource stream vẫn báo không hỗ trợ reset.
- Graphics.getFont/setFont lưu tham chiếu Font riêng từng Graphics; null trở về default. Việc render kiểu font/chữ chưa đầy đủ.

28 nhóm tích hợp + 120 mutation qua, gồm ca NUL, BMP delete, MIN/MAX long, Vector increment/doubling/negative capacity, mark/reset và font identity.
ARM signed binary qua các ca thư viện/MIDI, GameCanvas, JAD/monitor, UI và slice guard.
Ba game bị chặn được chạy lại desktop tối đa 4000 lượt và ARM tối đa 120 callback.
Kết quả và SHA-256 binary/tệp nguồn: `reports/compatibility/report.json` trong gói ZIP,
hoặc `build/upgrade2-final-20261005/report.md` trong dự án.

Ngân sách ARM 120 callback chưa đến các phím thử ở callback 350. Không suy từ kết quả này
rằng đã chơi hoàn chỉnh. Lượt 42 JAR/400 callback trước đó thuộc bản ký cũ, không phải
lượt kiểm thử toàn thư mục trên bản mới. CLDC/MIDP đầy đủ chưa đạt.


Kết quả bản ký mới:

| Game | Desktop 4000 lượt | ARM clock cố định 120 callback |
|---|---|---|
| Moto Racer Fury | Không lỗi, 2.089.191 opcode Java | Harness vượt trần instruction của callback |
| Mã Đao 2 | Vượt lỗi append(long), còn thiếu Class.forName | Không lỗi trong budget; hiển thị Chơi mới |
| Superman Returns | Vượt lỗi getFont/setLength, chạm trần bảng 8191 object | Không lỗi trong budget |

Moto được kiểm tra bổ sung với clock tiến 1 ms trên mỗi lần đọc SDK clock, thêm 33 ms
giữa các callback: 400 callback qua, heap sau quit = 0, màn hình vẫn Loading Java bytecode.
Đây là kiểm tra nhường CPU bằng clock tổng hợp, không so sánh với budget clock cố định
và không xác nhận vào gameplay. Kết quả riêng: `moto-timed.json`.
