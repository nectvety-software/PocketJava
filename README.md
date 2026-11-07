# PocketJava — JAR/JAD trên MRE VXP

Đã có lõi Java viết bằng C và `build/PocketJava.vxp`, cấu hình Nokia 225 /
MTK6260 / 240×320 / yêu cầu RAM 2048 KiB (heap theo dõi 1536 KiB trên MRE).

**Bản preview, chưa phải giả lập CLDC/MIDP đầy đủ.** JAR được đọc thật, class được
thực thi thật. Demo đã chạy trên desktop và binary ARM với API MRE mô phỏng.
Chưa thử Nokia 225 thật, chưa có FPS phần cứng và chưa xác nhận game thương mại.

## Dùng thử

1. Chép `build/PocketJava-signed.vxp` vào điện thoại hỗ trợ MRE;
   bản này đã ký RSA, Cert ID 100, IMSI `*`; chưa kiểm chứng trên firmware Nokia.
2. Đặt JAR/JAD ở thẻ nhớ hoặc bộ nhớ máy, kể cả thư mục con; ứng dụng tự quét cả hai ổ.
   Có thể dùng `E:\j2me\` cho demo và dữ liệu RMS.
3. Chép `demo/PocketDemo.jar` và `demo/PocketDemo.jad` vào thư mục đó.
4. Chọn **Built-in JAR demo** để thử ngay, hoặc chọn tệp trong danh sách.

Launcher theo phong cách RetroMRE: nền kem, header cam, hàng chọn màu đào, scrollbar và menu tùy chọn.
Phím: `2/8` chọn, `5/OK` mở, `4/6` đổi tab GAMES/JAR/JAD/INFO, softkey trái mở MENU,
`#` quét lại, softkey phải thoát ở launcher. Tab JAR/JAD lọc đúng loại tệp.
Trong MIDlet, `*` mở menu tạm dừng; `5/*` tiếp tục, `0` dừng về launcher.
Phím khác được gửi nguyên mã MRE/MIDP. Danh sách tối đa 64 tệp, tên/đường dẫn hỗ trợ Unicode BMP, đường dẫn UTF-8 ≤259 byte.
Quét từng lượt 16 mục; duyệt sâu tối đa 6 cấp, hàng đợi tối đa 96 thư mục.
Màn hình báo tiến độ hoặc giới hạn quét; hiển thị đường dẫn mục đang chọn.
Nhận `.jar`/`.jad` không phân biệt hoa thường, giữ các tệp trùng tên ở khác thư mục.
`#` quét lại; việc mở game giải phóng RAM/handle duyệt của scanner.

JAD phải có `MIDlet-Jar-URL` là tên JAR hoặc đường dẫn tương đối vào thư mục con; hỗ trợ `%20`/percent encoding và tên có dấu chấm. Chưa tải HTTP. Thuộc tính JAD được ưu tiên hơn manifest cho tên/main và
`getAppProperty`; kiểm tra `MIDlet-Jar-Size` nếu có, từ chối số sai định dạng hoặc kích thước không khớp. Hỗ trợ đuôi JAD trộn chữ hoa/thường. Chỉ chọn MIDlet đầu.

## Build

Cần Python 3, ARM GNU GCC/binutils, JDK hỗ trợ `javac --release 8`.
Build desktop cần GCC; script tự tìm MSYS2 MinGW64 ở `C:\msys64\mingw64\bin`.
SDK và miniz inflater đã được đặt trong `vendor/`.

```powershell
python tools/build.py all
python tools/build.py vxp
python tools/build.py vxp --imsi <IMSI>
```

Thay `<IMSI>` bằng IMSI 15 chữ số hoặc dạng Nokia 16 chữ số đã thêm `9` trước.
Script ghi nguyên input; chọn dạng theo firmware. Không dùng IMEI.
Output ký metadata: `build/PocketJava-signed.vxp`. Đây là binding IMSI theo packer
tham khảo, không phải chứng thư nhà sản xuất. Chưa xác nhận mở được trên Nokia 225.

```powershell
python -m pip install unicorn pyelftools Pillow
python tests/test_runtime.py
```

Chạy từ thư mục dự án sau build. Kết quả tại `build/test-report.json`,
`build/arm-smoke.json` và `build/arm-*.png`.

## Khả năng hiện tại

- ZIP stored/DEFLATE, central directory, CRC32 và giới hạn kích thước.
- Class loader, integer/reference/string constants, kế thừa, instance/static
  fields, constructor, class initializer, method calls, recursion và arrays.
- Tập con opcode integer/reference; GC mark/sweep giữ stack/static/string roots.
- Float: constants, locals, arrays, cộng/trừ/nhân/chia, đổi kiểu và so sánh (kể cả NaN); chưa có toàn bộ opcode float/double.
- MIDlet constructor/startApp/notifyDestroyed, Display.setCurrent/getCurrent, Canvas paint,
  keyPressed/keyReleased, getWidth/getHeight/isShown, repaint và getGameAction.
- Graphics setColor/fillRect/drawRect/drawString/drawRGB/translate; Math integer + long min/max,
  String.length/charAt/compareTo/concat/lastIndexOf/getChars/getBytes + byte[]/encoding constructors; Image.createRGBImage;
  Integer/Short numericValue helpers gồm intValue/shortValue/byteValue; Integer.toHexString/toOctalString/toBinaryString; DataInputStream.readChar/readFloat/readDouble/readLine/readUTF(DataInput) và skipBytes dùng scratch 64 byte; Thread.isAlive; System.out/PrintStream print/println compatibility sink; Random()/Random(long)/nextInt với state riêng theo instance.
- RGB565 framebuffer; tối đa 12000 opcode hoặc khoảng 8 ms mỗi callback,
  kiểm thời gian mỗi 64 opcode. Timer và yêu cầu repaint 33 ms.
  Lịch vẽ danh nghĩa khoảng 30 FPS, **chưa có số đo máy thật**.
  So sánh ARM cùng 60 callback AoE: xử lý 445500 thay vì 181830 opcode,
  flush 20 thay vì 12; peak heap giữ 574176 byte. Đây là throughput mỗi lượt trên
  clock mô phỏng, không phải mức tăng FPS phần cứng. Xem `docs/GAMEPLAY-PERFORMANCE.md`.

## Phần bổ sung và giới hạn

Bản sửa Nokia 225 Dual SIM: scratch giải nén JAR chuyển khỏi native stack sang heap
tracked, buffer lỗi ZIP/VM đồng nhất 160 byte, nạp superclass giới hạn 8 tầng.
Demo/library ARM qua với stack guard 8 KiB và các ca lỗi cấp phát khi mở Java.
Xem [bản sửa reboot](docs/NOKIA225-REBOOT-FIX.md); vẫn cần xác nhận trên điện thoại thật.

Bản sửa đọc tệp MRE (lỗi `Cannot read ZIP directory`): `vm_file_seek` dùng
`BASE_BEGIN` (=1) thay vì base `0`, kiểm tra `vm_file_getfilesize` trả 0 khi thành công,
và đọc lặp từng khối 1 KiB vì `vm_file_read` có thể trả ít hơn yêu cầu. Trước đó JAR
ngoài dừng ngay ở bước đọc EOCD, còn demo tích hợp vẫn chạy vì không dùng file API.
Mock ARM nay từ chối base 0, giả lập short read và mở JAR >64 KiB;
`build/arm-smoke.json` pass, heap sau quit = 0. VXPEmu đã qua bước mở ZIP/JAR;
chưa xác nhận trên máy thật. Xem [bản sửa file API](docs/MRE-FILE-IO-FIX.md).

Sau khi mở được JAR, VXPEmu dừng ở `Unsupported API` của `java/io/DataInputStream`.
Đã bổ sung `readChar`, `readFloat`, `readDouble`, `readLine`, bản tĩnh
`readUTF(DataInput)`, cho `readFully` đọc lặp theo khối và thêm opcode
`dcmpl`/`dcmpg` (151/152). Thanh trạng thái launcher xuống tối đa 3 dòng nên tên API
thiếu hiện đủ thay vì bị cắt ở 31 ký tự. Host test, mock ARM và bản ký pass;
heap sau quit = 0.

ARENA.jar nay chạy được qua màn hình `LỜI NÓI ĐẦU` và splash `LIÊN QUÂN MOBILE`
trên VXPEmu, tới menu đăng nhập khi chạy host. Ba lỗi lõi đã sửa: hằng
`static final String` qua `ConstantValue` (tag 8) bị từ chối, thiếu
`Display.flashBacklight`/`vibrate`, `Canvas.getKeyName` và lớp
`com/nokia/mid/ui/DeviceControl`; cấp phát object được thử lại sau GC. Game cần
khoảng 1,18 MiB heap đỉnh nên VXP nay yêu cầu 2048 KiB RAM. Xem
[bản sửa ARENA](docs/ARENA-JAR-FIX.md). Chưa xác nhận gameplay đầy đủ trên máy thật.

JAR/class được đọc và thực thi thật. Runtime chưa đạt CLDC/MIDP đầy đủ;
xem [ma trận coverage](docs/CLDC-MIDP-COVERAGE.md) để phân biệt phần đã kiểm tra,
phần còn thiếu và các API đang mô phỏng.

Cập nhật tương thích: Runtime.getRuntime/gc/freeMemory/totalMemory/exit, Thread.currentThread/getPriority/setPriority,
System.getProperty, Font face/style/size, String.getBytes với UTF-8/ASCII/Latin-1/UTF-16 và DataOutputStream.writeUTF (modified UTF-8).
Các khối synchronized dùng monitorenter/monitorexit có khóa reentrant và chờ hợp tác, tối đa 32 monitor đang khóa.
Chưa có wait/notify hay xử lý tự động flag synchronized của method. Priority được lưu/kiểm tra; scheduler vẫn round-robin.
Thuộc tính CLDC-1.1/MIDP-2.0 mô tả target API; đây vẫn là runtime tập con, chưa được chứng nhận CLDC/MIDP đầy đủ.
Runtime.freeMemory chỉ phản ánh ngân sách heap được theo dõi, không phải RAM trống toàn điện thoại.

Đợt nâng cấp tiếp theo thêm Long (parse/format, MIN/MAX, equals/hashCode),
readLong/writeLong big-endian, Short.parseShort kiểm tra tràn và Random.setSeed.
Đã bổ sung float/double return và wide locals, String.toString giữ identity,
Throwable.getMessage, StringBuffer.charAt theo Unicode BMP và kiểm tra bounds.
Resource InputStream báo markSupported=false, mark mặc định không làm gì và
reset ném IOException; Canvas super key callbacks dùng hành vi mặc định rỗng.

Đợt tiếp theo thêm StringBuffer.delete/deleteCharAt/setLength/append(long),
String.valueOf(char), Vector(initialCapacity, capacityIncrement) với tăng dung lượng
đúng theo increment/doubling và capacity(). ByteArrayInputStream hỗ trợ constructor
offset/length, mark/reset và close không vô hiệu hóa dữ liệu; DataInputStream ủy quyền
mark/reset xuống stream bên dưới. Graphics lưu/get Font theo từng instance; setFont(null)
khôi phục Font mặc định. Render font chữ vẫn là tập con. Xem docs/CORE-UPGRADE-2.md.
Image.createImage(InputStream) giải mã PNG từ stream với giới hạn kích thước hiện có.
Canvas thêm getKeyCode và truy vấn khả năng phím/pointer; Graphics thêm drawChar và gray scale.
GameCanvas có bộ đệm ảnh riêng, flush toàn màn hình hoặc vùng, paint mặc định và
getKeyStates giữ lần nhấn nhanh đến lần đọc kế tiếp. Bộ đệm indexed cần khoảng 76 KiB;
kiểm tra ARM xác nhận pixel không xuất hiện trước flush, flush vùng đúng và cleanup hết heap.

Đã thêm long, Thread/Runnable/sleep hợp tác/isAlive, switch/wide, exception tables/athrow,
mảng nhiều chiều, StringBuffer, Unicode BMP, Vector/Hashtable/Enumeration (gồm
Hashtable put/get/remove), stream byte/data (gồm InputStream.available), Font/clip, PNG/drawImage/drawRegion và RMS lưu tệp. JAR ngoài được đọc
trực tiếp; byte/char/short arrays được đóng gói. PNG palette giữ màu; ảnh truecolor
chuyển RGB332, alpha nhị phân để giảm RAM. Framebuffer RGB565 được ghi trực tiếp.

AoE đã thực thi Java, nạp ảnh và hiển thị hộp thoại; kết quả từng bản ký và từng
game lưu trong `build/full-compat-20261005/report.json`. Không lỗi trong budget
chưa chứng minh đã vào trận/chơi hoàn chỉnh. Chưa có số đo FPS Nokia thật.
Có alias Nokia FullCanvas và cắt vùng nhìn thấy trước khi giải mã drawImage/drawRegion.
Manager.playTone phát MIDI qua API MRE bằng bộ đệm cố định 48 byte (một tone cùng lúc),
dừng khi pause/hủy VM. Test ARM kiểm tra MIDI, chưa xác nhận âm thanh trên máy thật.
Player cho stream nhạc/SFX vẫn chỉ mô phỏng trạng thái; chưa phát MIDI/WAV/MP3 từ JAR.
Chưa có float/double đầy đủ, verifier/interface đầy đủ, mọi quy tắc array covariance,
Unicode ngoài BMP, Sprite/TileLayer, mạng, Form/List/Command và
API hãng/JSR bổ sung. Graphics vẫn chia sẻ trạng thái; text offscreen chưa hỗ trợ.
Native inflate/PNG/GC chưa chia lát thời gian. Không cam kết chạy mọi JAR.

Giới hạn: JAR 8 MiB, member 320 KiB, tracked VM heap 1536 KiB trên MRE và 896 KiB
trên host 64-bit (host giữ mức thấp để test tiết kiệm RAM), 95 classes. Constant pool
dùng đầy đủ `u2 constant_pool_count`; CP lưu offset 32-bit. Field/method dùng đầy đủ
`u2 fields_count/methods_count`; metadata method lưu code offset 32-bit và suy ra
exception-table. Object table được cấp phát lười theo page 128 slot, tối đa 8191 object,
page rỗng được GC thu hồi; không còn trần 1023 object cố định. GC mark/sweep không
đánh dấu lại node đã xử lý, nên đồ thị vòng được thu gom đúng. ZIP stream dùng input
scratch 1 KiB và dictionary 32 KiB. Java stack giữ trần logic 4096 slots/context nhưng
slots được cấp phát động theo nhu cầu; worker context được giải phóng sau khi `run()` kết thúc.
Giới hạn khác: 48 frames, 4 contexts, 16 phím chờ. RMS: 32 records/store, 64 KiB/record.
Heap không gồm framebuffer RGB565 150 KiB, code, native stack hay overhead firmware;
smoke-test Cops.L.A hiện đạt peak 994168 byte gồm framebuffer nhưng chạy đủ 1200 callbacks,
heap sau quit = 0. Full 29-game report gần nhất trước tối ưu này có avt3D peak 966568 byte
(tracked heap khoảng 812968 byte) và aoe-3 934568 byte.
Yêu cầu RAM 2 MiB (VXP) vẫn cần xác nhận trên máy thật.

## Ký và phân phối

Bản có chữ ký RSA: `build/PocketJava-signed.vxp`, Cert ID 100, IMSI `*`.
Backend VXPSign tại `D:\MRE\keystore\signing\re3vxpsigner`, khóa mặc định
`key\cert100-key.pem`. OpenSSL xác minh OK; byte thực thi giữ nguyên.
Không sao chép private key vào dự án hoặc gói phân phối.

```powershell
python tools/build.py all
python tests/test_runtime.py
python tools/sign_vxp.py
python tools/test_compat.py
python tools/test_gamecanvas.py
python tools/test_ui.py
python tools/package.py
```

Report `build/signing-report.json`, checksum `build/PocketJava-signed.vxp.sha256`.
Chữ ký hợp lệ chưa chứng minh firmware Nokia chấp nhận ứng dụng.

Gói cài đặt: `dist/PocketJava-signed-package.zip` (VXP đã ký, demo JAR/JAD, README,
docs và reports) kèm `dist/PocketJava-signed.vxp` và các tệp `.sha256`.
Kiểm tra bằng `sha256sum -c <tệp>.sha256`; gói không chứa khóa ký.

## Mã nguồn

- `native/vm.c`: interpreter, class loader, GC, native MIDP.
- `native/jar.c`: JAR/JAD và inflater bridge.
- `native/mre.c`: launcher, file browser, màn hình, timer, phím, lifecycle MRE.
- `tools/build.py`: build VXP; `demo/java/PocketDemo.java`: demo bytecode thật.
- `demo/api`: compile-only stubs, **không** đóng vào demo JAR.
- `docs/PLAN.md`: lộ trình hoàn thiện và tiêu chí hiệu năng.
- `docs/VALIDATION.md`: kết quả test và cách thử máy thật.

Mẫu LuaS30 cũ (`main.lua`, `src/*.lua`, `project.json`, `.luas30`) được giữ nguyên.
Build native không dùng chúng. Nút build Lua của IDE vẫn build mẫu cũ;
để xuất giả lập dùng `python tools/build.py vxp`. README cũ lưu ở
`docs/README-lua-template.md`.

## Nguồn tham khảo

- [mre-makefile](https://github.com/gtrxAC/mre-makefile), commit
  `ca2c07b569fe1c855aaea6ab4920c0d927fce46a`: SDK và packer; upstream chỉ xác nhận MRE 3.x.
- [Pico JVM](https://github.com/orenskl/pico-jvm): tham khảo kiến trúc; không dùng mã.
- [Oracle JVM opcodes](https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-6.html),
  [MIDlet](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/javax/microedition/midlet/MIDlet.html),
  [Canvas](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/javax/microedition/lcdui/Canvas.html),
  [Graphics](https://docs.oracle.com/javame/config/cldc/ref-impl/midp2.0/jsr118/javax/microedition/lcdui/Graphics.html).
- [miniz 3.0.2](https://github.com/richgel999/miniz/tree/3.0.2): inflater.

Giấy phép: `vendor/miniz/LICENSE`, `vendor/mre/LICENSE` và các header SDK.
MIT của build script không thay thế điều khoản riêng của thư viện/header MediaTek.

Quét tham khảo thuật toán hàng đợi thư mục của dự án RetroMRE tại
`C:\Users\doxuanhop\Documents\VXP Projects\RetroMRE\main\romfs.cpp`.
Kiểm thử ARM bổ sung: `python tools/test_scanner.py`, report `build/scanner-test.json`.
