# Sửa `Cannot read ZIP directory` khi mở JAR ngoài

## Nguyên nhân

`platform_read_at` trong `native/mre.c` gọi:

```c
vm_file_seek((VMFILE)file, (VMINT)offset, 0)
```

`vmio.h` chỉ định nghĩa `BASE_BEGIN` = 1, `BASE_CURR` = 2, `BASE_END` = 3.
Giá trị `0` không phải origin hợp lệ, nên bước seek về cuối tệp để đọc EOCD
thất bại và `jar_open_file` trả `Cannot read ZIP directory`.

Demo tích hợp nằm trong `demo_jar.h` và được nạp bằng `pj_load_memory`, không đi
qua file API, nên vẫn chạy trong khi mọi JAR trên thẻ/điện thoại đều lỗi.

Cùng nhánh này còn hai giả định chưa đúng với API MRE:

- `vm_file_getfilesize` trả **0** khi thành công, nhưng code chỉ kiểm tra `< 0`.
- `vm_file_read` có thể trả ít byte hơn yêu cầu; code cũ đọc một lần và đòi đủ.

## Sửa

- `vm_file_seek(..., BASE_BEGIN)` và kiểm tra kết quả `!= 0`.
- Kiểm tra `vm_file_getfilesize(...) != 0` là lỗi.
- `read_exact` đọc lặp theo khối 1 KiB, dựa trên `nread` thực tế, dùng chung cho
  `platform_read_at` và `platform_read` (JAD/RMS).

## Kiểm tra

- `tools/arm_smoke.py` mock MRE nay đúng hợp đồng SDK: chỉ nhận base 1/2/3,
  trả `-1` khi seek sai, `vm_file_read` trả tối đa 997 byte mỗi lượt, và JAR mặc
  định được đệm lên hơn 64 KiB để chạy nhánh đọc EOCD ở cuối tệp.
- Với mock cũ (chấp nhận base 0, đọc đủ một lần), lỗi không tái hiện; với mock
  chặt, bản ARM cũ lỗi đúng `Cannot read ZIP directory`, bản đã sửa mở được JAR
  ngoài và JAD.
- `build/arm-smoke.json`: `external_jar`, `external_jad` pass; heap sau quit = 0.
- `tests/test_runtime.py`: 31 nhóm integration + 120 mutation pass.

## Xác nhận trên VXPEmu và lỗi kế tiếp

VXPEmu đã mở được JAR ngoài: thông báo `Cannot read ZIP directory` không còn xuất hiện.
Lỗi kế tiếp là `Unsupported API` của `java/io/DataInputStream` (game đọc dữ liệu nhị phân).
Đã bổ sung `readChar`, `readFloat`, `readDouble`, `readLine`, bản tĩnh
`readUTF(DataInput)`, cho `readFully` đọc lặp theo khối và thêm `dcmpl`/`dcmpg`.
Thanh trạng thái launcher xuống 3 dòng để hiện đủ tên API còn thiếu.

## Giới hạn

Đây là kiểm tra ARM với API MRE mô phỏng. Chưa chạy lại trên Nokia 225 thật; game đã
dùng để thử trên VXPEmu chưa được xác nhận chạy hết sau khi bổ sung DataInputStream.
