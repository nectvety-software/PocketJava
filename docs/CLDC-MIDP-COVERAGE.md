# CLDC 1.1 / MIDP 2.0: trạng thái thực thi

JAR được đọc qua ZIP stored/DEFLATE thật; class được nạp và bytecode được
interpreter C thực thi trên desktop và ARM VXP. Demo không thay thế nội dung
game. Các API chưa hỗ trợ báo lỗi rõ ràng. Runtime hiện chưa đạt CLDC/MIDP đầy đủ.

Tham chiếu phạm vi:
[CLDC 1.1](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/cldc11api.pdf),
[MIDP 2.0](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/overview-summary.html).
Tên profile trả bởi System.getProperty là target, không phải chứng nhận tuân thủ.

| Nhóm | Đã thực hiện / kiểm tra | Phần phải hoàn thiện |
|---|---|---|
| VM/class | class loader, fields/calls, kế thừa, arrays, GC, exceptions, int/long và tập con float/double | verifier, interface/covariance đầy đủ, opcode số thực còn lại, synchronized method, wait/notify |
| java.lang | String/StringBuffer và charAt bounds, String.toString identity, Throwable.getMessage, numeric helpers, Runtime, System properties, cooperative Thread/Runnable | đầy đủ overload và quy tắc Unicode, thư viện số thực, scheduler/monitor semantics đầy đủ |
| java.io/util | byte/data streams, readChar/readFloat/readDouble/readLine/readLong, readUTF(DataInput), writeLong/writeUTF, PNG InputStream, ByteArrayInputStream mark/reset/offset/close, DataInputStream mark delegation, Vector capacity/increment, Hashtable/Random | Reader/Writer, Date/Calendar/Timer và semantics stream đầy đủ; Random hiện dùng generator riêng |
| MIDlet/lifecycle | constructor/startApp, Display.setCurrent, pause/resume/quit launcher, notifyDestroyed | pauseApp/destroyApp gọi đúng mọi chuyển trạng thái, quản lý suite và security permissions đầy đủ |
| lcdui | Canvas, Font, clip, Image PNG/RGB, Graphics cơ bản và transform, Font reference riêng từng Graphics | Form/List/Alert/Command/Item, Graphics color/clip state riêng và text offscreen, toàn bộ primitive/anchor semantics |
| lcdui.game | GameCanvas buffer, flush vùng/toàn màn hình, paint mặc định, key latch/suppression | Sprite/TileLayer/LayerManager, collision và các trạng thái GameCanvas còn lại |
| RMS | store/record lưu tệp theo suite | enumeration/filter/comparator/listener và toàn bộ lỗi/quota/lifecycle semantics |
| Media | playTone gọi MIDI MRE, dừng khi pause/quit | Player stream MIDI/WAV/MP3 thực, controls/listeners và timing đầy đủ; Player hiện mô phỏng trạng thái |
| IO/network/PKI | chưa triển khai | Connector/protocols, HTTP/HTTPS/socket/datagram, certificates/security và quyền người dùng |
| API hãng/JSR | Nokia FullCanvas và một số helpers | coverage theo từng game; không thuộc toàn bộ API MIDP chuẩn |

Kiểm chứng hiện có: 28 nhóm tích hợp, 120 class mutation; ARM signed binary,
PNG/graphics pixel oracle, MIDI API payload, monitor contention, JAD percent paths,
GameCanvas buffer isolation/flush/default paint/key suppression, UI và slice guard.
Tệp game nguồn được hash trước/sau. Báo cáo thư mục lưu lỗi từng JAR và số callback.
`tools/inventory_game_apis.py` đọc constant pool của toàn bộ class để liệt kê các
tham chiếu class/method, kể cả nhánh chưa chạy. Class chưa đăng ký có thể nằm trong
nhánh tùy chọn; class đã đăng ký vẫn có thể thiếu method hoặc semantics. Danh sách
tham chiếu không phải tỷ lệ tương thích hoặc chứng nhận tuân thủ.

Không lỗi trong budget chưa xác nhận gameplay hoàn chỉnh. Thử nghiệm Unicorn sử
dụng API MRE mô phỏng; chưa có bằng chứng FPS/âm thanh/firmware trên điện thoại.
Không gọi runtime là đầy đủ trước khi đóng các khoảng trống và chạy kiểm chứng
semantics/API tương ứng; không thay API còn thiếu bằng no-op để tăng tỷ lệ pass.
