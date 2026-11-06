# Sửa crash khi mở Java — Nokia 225 Dual SIM

Phát hiện và sửa hai lỗi cụ thể trong mã nguồn:

- inflate_file đặt tinfl_decompressor và input[4096] trên native stack. Chuyển cả hai sang heap tracked, input giảm còn 1024 byte; giải phóng ở mọi nhánh thoát.
- Đường nạp JAR từ memory gọi wrapper miniz có stack frame 8400 byte. Thay bằng state DEFLATE cấp phát heap, dùng chung giới hạn RAM.
- Hàm báo lỗi ZIP ghi tối đa 160 byte, trong khi VM chỉ dành 96 byte. Đồng nhất PJ_ERROR_CAP=160; thêm kiểm tra marker title bên cạnh không bị ghi đè.
- Nạp superclass lặp đệ quy có thể làm tràn native stack. Giới hạn 8 tầng nạp class; trả lỗi rõ ràng và dọn VM khi vượt.

GCC ARM stack-usage sau sửa: inflate_file 80 byte, jar_extract 376 byte;
tinfl_decompress 304 byte. Wrapper miniz 8400 byte vẫn thuộc vendor nhưng không còn
được gọi từ đường nạp JAR của ứng dụng. Chỉ số từng hàm không phải tổng stack call chain.

Kiểm tra bản ARM đã ký với chỉ 8 KiB native stack được map (vùng bên dưới bị guard):
PocketDemo và library/PNG đều hoàn tất; heap sau quit = 0. Giả lập lỗi tại 16 lần
cấp phát khác nhau trong luồng mở Java: không crash và heap sau quit = 0 ở mọi ca.
30 nhóm tích hợp và 120 mutation qua, gồm lỗi ZIP và class kế thừa quá sâu.
GameCanvas/JAD/monitor/UI/slice guard cũng qua trên bản ký này.

Report: build/launch-safety.json, build/test-report.json, build/signing-report.json.
Đây là xác minh ARM với API MRE mô phỏng. Chưa tái hiện trực tiếp reboot trên Nokia
225 Dual SIM, nên chưa xác nhận đã khắc phục mọi nguyên nhân firmware/thiết bị.
Ngân sách tracked VM heap không đổi; thiếu RAM vẫn phải trả lỗi thay vì truy cập NULL.

Chép PocketJava-signed.vxp mới vào máy, đóng ứng dụng cũ và mở lại. Thử Built-in Demo
trước, rồi JAR từng gây reboot. Báo tên JAR và thời điểm reboot nếu vẫn tái diễn.
