# Kiểm tra — 02/10/2026

14 nhóm integration và 120 class mutations đã qua: JAR/JAD, CRC/bounds,
GC 12000 allocations, kế thừa/arithmetic, long/threads/mảng nhiều chiều/Unicode/
streams/collections/exceptions. Binary ARM trong VXP chạy qua Unicorn với mock
MRE APIs: SDK resolver, file API, phím, pause/hide/resume/quit; heap sau quit = 0.
Report: build/test-report.json, build/arm-smoke.json.

Sau sửa constant-pool, chạy lại toàn bộ 29 JAR tại `D:\\Program\\gamejava\\j2me`:
không còn `Constant pool limit reached` hoặc `Memory limit: constant pool` ở host/ARM.
Các game trước đây bị chặn tại CP nay đi tiếp tới lỗi kế tiếp (field/API/class/opcode/heap).
Sau sửa Field/Method limit, chạy lại toàn bộ 29 JAR: không còn `Field limit reached`,
`Method limit reached` hoặc `Memory limit: methods`. Sau bước sửa heap/object tiếp theo,
batch 29 JAR cũng không còn `Java heap/object limit`, `Java object table exhausted`,
`ZIP stream RAM limit`, `Output stream RAM exhausted`, `Memory limit: constant pool`
hay `Memory limit: methods` trong report cuối. Object table chuyển sang page 256 slot
cấp phát lười (tối đa 8191 object), GC sửa mark-cycle và thu hồi page rỗng; ZIP stream
giảm input scratch từ 4 KiB xuống 1 KiB. Heap cap target MRE là 768 KiB; host 64-bit
896 KiB để bù pointer metadata. Sau bổ sung `Display.getCurrent()`, chạy lại đủ 29 JAR:
không còn lỗi `Display.getCurrent` trong host/ARM report. Sau bổ sung
`java/util/Hashtable.remove(Ljava/lang/Object;)Ljava/lang/Object;`, chạy lại đủ 29 JAR
và không còn lỗi `Hashtable.remove` trong host/ARM report. `avt3D240x320s40v5.jar`
đi tiếp tới `String.compareTo(String)`; `Cops.L.A.jar` và
`spider-man-240x320-s60.jar` đi tiếp tới `Random.<init>(J)`. Regression collections
cũng kiểm tra put/get/remove và trả value cũ/null đúng theo trường hợp test. Sau bổ sung
`String.compareTo(String)`, chạy lại đủ 29 JAR và không còn lỗi `String.compareTo`
trong host/ARM report. `avt3D240x320s40v5.jar` đi tiếp tới `Random.<init>(J)`,
`DungSi-BauTroi.jar` tới `String.lastIndexOf(I)`, và `thachsanh.jar` tới `System.out`.
Regression kiểm tra compareTo với âm/0/dương, prefix và Unicode BMP. Sau bổ sung
`Random.<init>(J)`, chạy lại đủ 29 JAR và không còn lỗi `Random.<init>(J)` trong
host/ARM report. Random giữ state riêng theo instance; seed 64-bit được fold vào LCG
32-bit hiện có và regression xác nhận hai instance cùng seed sinh cùng chuỗi. `AssassinsCreed.jar`
chạy hết host budget 4000 bước (15997528 lệnh) và ARM 1200/1200 callbacks không lỗi,
peak mock gồm framebuffer 574736 bytes, heap sau quit = 0. `avt3D240x320s40v5.jar`
và `Cops.L.A.jar` đi tiếp tới `Math.min(JJ)J`; `spider-man-240x320-s60.jar` đi tiếp
tới thiếu `com/nokia/mid/ui/DeviceControl`. Sau bổ sung `Math.min(JJ)J`, regression
kiểm tra cả long dương có high-word khác nhau và long âm; chạy lại đủ 29 JAR không còn
`Math.min(JJ)J` trong host/ARM report. `avt3D240x320s40v5.jar` và `Cops.L.A.jar`
đều đi tiếp tới `Math.max(JJ)J`. Sau bổ sung `Math.max(JJ)J`, regression kiểm tra
long dương có high-word khác nhau và long âm; chạy lại đủ 29 JAR không còn
`Math.max(JJ)J` trong host/ARM report. `avt3D240x320s40v5.jar` đi tiếp tới
`String.<init>([BIILjava/lang/String;)V` (ARM 15 callbacks, peak mock 901584 bytes);
`Cops.L.A.jar` đi tiếp tới `Image.createRGBImage([IIIZ)` (ARM 30 callbacks, peak mock
565656 bytes). `AssassinsCreed.jar` vẫn chạy hết host budget và ARM 1200/1200 callbacks
không lỗi, peak mock 574736 bytes; `aoe-3-240x320.jar`
vẫn 1200/1200 callbacks, peak mock 934488 bytes. Cả hai heap sau quit = 0.
Corpus test được xác nhận không đổi (`source_files_unchanged: true`). Sau đó bổ sung
`String.<init>([BIILjava/lang/String;)V` với UTF-8/ISO-8859-1/US-ASCII/UTF-16 BE/LE
và `Image.createRGBImage([IIIZ)` chuyển ARGB8888 sang RGB565 + alpha mask. Regression
14 nhóm + 120 mutation vẫn pass. Smoke-test sau `String.<init>([BIILjava/lang/String;)V` + `Image.createRGBImage([IIIZ)`:
`avt3D240x320s40v5.jar` host đi tới `Graphics.drawRGB([IIIIIIIZ)V`; ARM đạt 58 callbacks
rồi dừng ở `Java heap exhausted (696061/786432, need 112768)`, peak mock 901592 bytes,
heap sau quit = 0. Sau bổ sung `Graphics.drawRGB([IIIIIIIZ)V`, regression 14 nhóm +
120 mutation vẫn pass. Smoke-test sau drawRGB: host `avt3D` đi tới `Integer.byteValue()B`
sau 1363189 lệnh và 18184 lệnh vẽ; ARM đạt 58 callbacks rồi dừng ở cùng lỗi heap
`696061/786432, need 112768`, peak mock 901592 bytes, heap sau quit = 0. Sau bổ sung
`Integer.byteValue()B` với sign-extension byte và chuẩn hóa `shortValue()` về signed 16-bit,
regression 14 nhóm + 120 mutation tiếp tục pass. Smoke-test mới `avt3D`: host chạy hết
4000-step budget không lỗi, 15495281 lệnh và 312172 lệnh vẽ, peak host 821814 bytes;
ARM vẫn đạt 58 callbacks và dừng ở đúng allocation heap `696061/786432, need 112768`,
peak mock 901592 bytes, heap sau quit = 0. Phân tích bytecode xác định allocation là
`new int[28192]` trong scratch buffer static của class `c` (112768 bytes). Sau tăng riêng
MRE tracked heap từ 768 KiB lên 816 KiB, regression 14 nhóm + 120 mutation vẫn pass.
Smoke-test `avt3D` ban đầu được kéo tới 240 callbacks: ARM chạy đủ 240/240 không lỗi trong
budget. Sau đó chạy dài 1200 callbacks trên cùng cap 816 KiB và cùng bản ký: ARM chạy đủ
1200/1200 callbacks, không lỗi trong budget, wall time 143.188 s. Peak mock gồm framebuffer
966568 bytes, tương đương tracked heap khoảng 812968 bytes, còn khoảng 22616 bytes headroom
dưới cap 816 KiB; heap sau quit = 0. Các mốc heap: callback 100=966448, 400=966488,
700=966528, 1000=966568 và giữ ổn định tới 1200; chưa thấy scratch buffer thứ hai hay leak
đáng kể. Không có blocker API/RAM mới trong budget 1200. Report riêng:
`build/avt3d-816k-smoke-1200/`.
Sau đó chạy lại full 29-game corpus với cap MRE 816 KiB. Baseline pre-816 được lưu tại
`build/baseline-pre816/`. So sánh ARM: 2/29 game cải thiện, 27/29 giữ nguyên, 0 regression;
số game không lỗi trong budget tăng 2 -> 3, host tăng 1 -> 2. `avt3D240x320s40v5.jar`
tăng từ 15 callbacks (blocker String(byte[],encoding)) lên 1200/1200 không lỗi, peak
966568 bytes. `Cops.L.A.jar` tăng từ 30 lên 397 callbacks, peak 936144 bytes và blocker
khi đó là `Thread.isAlive()Z`. Sau bổ sung `Thread.isAlive()Z`, context worker giữ ref tới
object Thread trong lúc sống, GC mark ref này và xóa khi `run()` kết thúc; regression kiểm
tra false trước start, true trong run và false sau khi worker kết thúc. Smoke-test sau `Thread.isAlive()` ban đầu: `Cops.L.A.jar` ARM đi tới 409 callbacks rồi
dừng ở `Java heap exhausted (833744/835584, need 4534)`, peak mock 992008 bytes.
Sau tối ưu RAM bằng object-page 128 slot, Java stack/context cấp phát động và giải phóng
worker context sau `run()`, không tăng cap MRE: host chạy hết 4000-step budget không lỗi
(15689946 lệnh, peak 917460 bytes); ARM chạy đủ 1200/1200 callbacks không lỗi trong budget,
peak mock gồm framebuffer 994168 bytes, heap sau quit = 0. Report:
`build/systemout-cops-smoke-cops/`. Sau bổ sung
`String.lastIndexOf(I)I` (kèm `(II)I`) theo chỉ số Unicode BMP, `DungSi-BauTroi.jar`
đi tiếp từ `lastIndexOf` tới `System.out`. Sau bổ sung `System.out` singleton +
`java/io/PrintStream` compatibility sink cho print/println/flush/close/checkError, bốn game
vượt blocker `System.out`: `Audition.jar` -> `String.getChars(II[CI)V` (ARM 1 callback,
peak 226360); `DungSi-BauTroi.jar` -> `Manager.createPlayer(InputStream,String)` (ARM 4,
peak 443696); `thachsanh.jar` -> `h.getGraphics()` (ARM 2, peak 323760); và
`vua-danh-lon-3-240x320.jar` -> `InputStream.available()I` (ARM 13, peak 483664).
Sau bổ sung `String.getChars(II[CI)V` theo chỉ số Unicode BMP + bounds Java và
`InputStream.available()I` cho ByteArrayInputStream/DataInputStream/JAR stream không tiêu thụ dữ liệu,
`Audition.jar` đi tiếp tới `String.getBytes()[B` (ARM 1 callback, peak 243416) và
`vua-danh-lon-3-240x320.jar` đi tiếp tới `String.<init>([BLjava/lang/String;)V` (ARM 13,
peak 484584). Sau bổ sung `String.getBytes()[B` xuất UTF-8 theo representation nội bộ và
`String.<init>([BLjava/lang/String;)V` tái sử dụng decoder charset hiện có, regression
round-trip `AéZ` qua UTF-8 pass. Smoke-test mới: `Audition.jar` tăng lên 75 callbacks,
blocker mới `_B_B.isShown()Z`, peak 288160; `vua-danh-lon-3-240x320.jar` tăng lên 20
callbacks, blocker mới `String.concat(Ljava/lang/String;)Ljava/lang/String;`, peak 495768.
Cả hai heap sau quit = 0. Sau bổ sung `String.concat(Ljava/lang/String;)Ljava/lang/String;`
và `Displayable/Canvas.isShown()Z`, regression kiểm tra concat Unicode/empty string và
isShown false trước `setCurrent`, true sau `setCurrent`. `Audition.jar` vượt `_B_B.isShown()`
(do `_B_B` kế thừa `Canvas`) và chạy đủ ARM 1200/1200 callbacks không lỗi trong budget,
peak mock 947240 bytes, heap sau quit = 0; host cũng hết 4000-step budget không lỗi.
`vua-danh-lon-3-240x320.jar` vượt `String.concat`, ARM vẫn 20 callbacks nhưng blocker mới
là `Integer.parseInt(Ljava/lang/String;I)I`, peak 495768 bytes, heap sau quit = 0.
Reports: `build/concat-isshown-smoke-audition/` và `build/concat-isshown-smoke-vuadanhlon/`.
`aoe-3-240x320.jar` và `AssassinsCreed.jar` vẫn 1200/1200,
peak lần lượt 934568 và 574736 bytes. Tất cả 29 ca có heap sau quit = 0; corpus nguồn
không đổi. Report tổng hợp: `build/gamejava-current/`; bảng so sánh:
`build/gamejava-current/comparison-pre816-vs-816.md`. Đây là harness, chưa xác nhận
gameplay hay máy Nokia thật.

Đóng gói và ký bằng VXPSign tại D:\MRE\keystore\signing\re3vxpsigner.
RSA-SHA1 PKCS#1 v1.5, Cert ID 100, IMSI *, OpenSSL VERIFY OK. So sánh xác nhận
phần executable không đổi. Report: build/signing-report.json; có SHA-256 sidecar.
Không đưa private key vào gói.

AoE 404588 bytes: đã qua tải JAR, tạo MIDlet/luồng, giải mã ảnh, chọn ngôn ngữ và
nạp tài nguyên. Bản ARM hiện chạy đủ 1200/1200 callbacks không lỗi trong budget test;
peak heap mock gồm framebuffer là 934568 bytes và heap sau quit = 0. Host 64-bit đi
sâu hơn trước nhưng hiện dừng ở `Invalid / null object reference`, không còn lỗi memory.
Chưa chứng minh vào trận/chơi được. Không sửa JAR gốc; đây không phải RAM/FPS đo
trên Nokia thật.

Chưa xác nhận Nokia 225/firmware, khóa tin cậy, MRE ABI/GDI, RAM thực hoặc FPS.
Unicorn thực thi ARM nhưng API MRE và font renderer được mô phỏng.

Thử máy thật: chép VXP đã ký; tạo E:\j2me; đặt JAR và JAD cùng thư mục. JAD trỏ
tên JAR local. Mở demo, kiểm tra phím/pause/resume và ghi lại firmware/kết quả mở.

Scanner mới đã qua kiểm thử ARM: quét thư mục con trên cả hai ổ, JAR/JAD viết hoa,
Unicode BMP, tệp trùng tên khác đường dẫn, mở JAD lồng nhau, # quét lại, danh sách
rỗng, 64 tệp/cuộn trang, hàng đợi 96 thư mục, sâu 6 cấp, hide/resume và tháo thẻ.
Quét 16 mục/lượt; giải phóng queue/handle trước khi mở Java hoặc thoát.
Report build/scanner-test.json; ảnh build/arm-scanner-list.png.
