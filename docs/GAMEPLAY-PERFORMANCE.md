# Tăng lượng xử lý gameplay — 04/10/2026

Lịch MRE và host dùng chung cấu hình: 12000 opcode/lượt, giới hạn 8 ms,
timer/repaint 33 ms. Trước đây 4000 opcode/lượt và repaint 66 ms.
Giới hạn opcode cao hơn tận dụng thời gian còn trống trên thiết bị nhanh;
thiết bị chậm vẫn dừng theo clock. Native PNG/inflate/GC chưa chia lát và
có thể vượt 8 ms; chưa có cam kết độ trễ/FPS điện thoại.

Thread.isAlive được sửa để không báo sống sau khi run() đã trả về,
kể cả khi scheduler chưa giải phóng context. Lượt xử lý dài hơn đã bộc lộ lỗi này.

## Đo cùng AoE, cùng 60 callback ARM

| Chỉ số | Trước | Sau |
|---|---:|---:|
| Java opcode thực thi | 181830 | 445500 |
| Flush màn hình | 12 | 20 |
| Peak heap gồm framebuffer | 574176 byte | 574176 byte |
| Heap sau thoát | 0 | 0 |
| Tổng thời gian callback Unicorn | 9329.7 ms | 20450.7 ms |

Lượng logic mỗi 60 callback tăng 2.45 lần; số flush tăng 1.67 lần.
Unicorn sử dụng clock mô phỏng cố định trong mỗi callback nên không kích hoạt
deadline 8 ms như máy thật. Bản mới làm nhiều việc hơn và tổng thời gian chạy
Unicorn cũng tăng. Không diễn giải 2.45 lần thành tốc độ CPU/FPS phần cứng.
Đây là benchmark vòng xử lý/load/render, chưa chứng minh gameplay đầy đủ.

## Kiểm chứng

- 22 nhóm tích hợp và 120 class mutation qua; demo ARM có phản hồi phím,
  pause, hide/resume, JAR/JAD, quét tệp và cleanup.
- Test clock ARM tăng 1 ms mỗi lần đọc: các slice thực thi 257–314 opcode,
  dừng trước trần 12000, heap sau thoát = 0.
- Test pixel 8 transform/clipping và MIDI vẫn qua.
- Reports: build/gameplay-before.json, build/gameplay-after.json,
  build/gameplay-slice-test.json, build/test-report.json.

Báo cáo 29 game ở session-20261004-final thuộc bản trước thay đổi lịch chạy;
không dùng báo cáo đó để chứng nhận binary hiện tại. Các lỗi API chưa được
sửa bởi thay đổi hiệu năng này. Player nhạc/SFX từ stream vẫn mô phỏng.
