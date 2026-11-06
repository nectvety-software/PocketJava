# Kiểm thử Ninja School 3 và sửa lõi

Nguồn: `D:/Program/gamejava/j2me/NinjaSchool3_240x320.jar`, Ninja School 3 1.3, CLDC 1.0 / MIDP 2.0.
JAR gốc được giữ nguyên. Lõi đọc ZIP/class và thực thi bytecode Java thực.

Hai lỗi đã sửa:

- Font `fb.png` có kích thước 11 × 1885 và `fr.png` 10 × 1692. Giới hạn 1024 pixel mỗi chiều trước đây làm tải font thất bại, sau đó vẽ ảnh null. PNG giờ giới hạn diện tích giải mã ở `PJ_ENTRY_LIMIT/3`, mỗi chiều tối đa 32767; không tăng ngân sách heap.
- `u/tt.png` thực chất là GIF89a 135 × 40. Lõi nhận dạng theo nội dung, giải mã LZW khung đầu với bảng màu, transparency và interlace. Dictionary 4096 mục đặt trên heap, giải phóng cả khi lỗi; không đặt trên stack callback Nokia.

Giải mã GIF hiện phục vụ ảnh tĩnh, không chạy animation nhiều khung. Độ chính xác màu sử dụng RGB565 và alpha nhị phân như lõi ảnh hiện có. Các ảnh khác PNG/GIF chưa được bổ sung trong lần sửa này.

Kiểm thử pixel đối chiếu toàn ảnh với Pillow: 3 GIF tạo riêng (bao gồm interlace, transparency và tăng dictionary), GIF thật trong game, hai font PNG thật. 60 ảnh GIF cắt cụt đều kết thúc có giới hạn và không rò heap.

Bộ hồi quy gồm 31 nhóm, 120 class mutation; thêm kiểm tra ARM GameCanvas, UI, monitor/JAD, scheduling, stack callback 8 KiB và 16 điểm cấp phát thất bại. Báo cáo liên kết với bản VXP nằm trong gói phát hành.

Giới hạn xác nhận: ARM chạy bằng Unicorn với API MRE mô phỏng; chưa kiểm tra trực tiếp Nokia 225 Dual SIM, chưa đo FPS phần cứng. Player phát nhạc/SFX từ stream vẫn chưa có triển khai âm thanh thật. Không coi chạy hết ngân sách tick là hoàn thành gameplay hoặc toàn bộ cốt truyện.

Tham khảo thuật toán: [GIF89a specification](https://www.w3.org/Graphics/GIF/spec-gif89a.txt). GIF là service mark của CompuServe.
