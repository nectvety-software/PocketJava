"""Summarize compatibility evidence for the matching signed VXP."""
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'build/compat-20261004-final';r=json.loads((folder/'report.json').read_text(encoding='utf8'))
    assert r.get('source_files_unchanged'),'Run incomplete'
    assert r['signed_vxp_sha256']==hashlib.sha256((ROOT/'build/PocketJava-signed.vxp').read_bytes()).hexdigest()
    files=r['files'];host=[Path(x['path']).name for x in files if x['host'].get('ok')]
    arm=[Path(x['path']).name for x in files if x['arm'].get('no_error_within_budget')]
    cleanup=sum(x['arm'].get('heap_after_quit')==0 for x in files)
    lines=['# Nâng cấp tương thích JAR/JAD — 04/10/2026','',
           f"Thư mục `{r['source']}`: {r['jar_count']} JAR, {r['jad_count']} JAD. SHA-256 tệp game giữ nguyên.",'',
           '## Lõi mới','',
           '- Runtime singleton, gc/freeMemory/totalMemory/exit; số RAM là ngân sách tracked heap.',
           '- Thread.currentThread/getPriority/setPriority; priority lưu và kiểm tra 1–10, scheduler vẫn round-robin.',
           '- Khối synchronized: monitorenter/monitorexit, khóa reentrant, chờ giữa context và đánh thức khi nhả khóa. Tối đa 32 monitor đang khóa.',
           '- System.getProperty: platform/encoding/locale/configuration/profiles; thuộc tính chưa biết trả null.',
           '- Font face/style/size và isPlain/isBold/isItalic/isUnderlined.',
           '- String.getBytes với UTF-8, US-ASCII, ISO-8859-1, UTF-16/BE/LE trong phạm vi Unicode BMP.',
           '- DataOutputStream.writeUTF: modified UTF-8, NUL hai byte, kiểm tra trần 65535 byte, scratch 64 byte.',
           '- JAD đường dẫn thư mục con, percent encoding như %20, mixed-case extension. Từ chối URL mạng, đường dẫn tuyệt đối và segment ..; dùng dấu phân cách MRE trên ARM.','',
           '## Kết quả','',
           '28 nhóm tích hợp + 120 class mutation qua. ARM kiểm tra lock contention, JAD tương đối/encoded, MIDI, pixel, UI và cleanup.',
           f'Desktop: {len(host)}/{len(files)} không lỗi trong 4000 lượt; ARM VXP ký: {len(arm)}/{len(files)} không lỗi trong 120 callback.',
           f'Heap sau quit = 0: {cleanup}/{len(files)} tệp. Peak ARM gồm framebuffer: '+str(max(x['arm'].get('heap_peak_including_framebuffer',0) for x in files))+' byte.',
           'Clock/API MRE là mô phỏng; các budget khác nhau và ARM chưa đến mốc phím 350/650/950. Không xác nhận gameplay đầy đủ, FPS hay firmware điện thoại.','',
           '## Không lỗi trong budget ARM','',*['- '+x for x in arm],'',
           '## Giới hạn còn lại','',
           'Chưa xử lý tự động flag synchronized trên method, Object.wait/notify, Form/Command, API hãng đầy đủ, mạng, stream nhạc/SFX hoặc toàn bộ CLDC/MIDP. Các thuộc tính CLDC/MIDP mô tả target API, không phải chứng nhận tuân thủ đầy đủ.',
           'JAR tối đa 8 MiB, member 320 KiB và RAM vẫn giữ giới hạn cũ. Unsupported API/opcode và hết heap vẫn được báo, không coi mock im lặng là tương thích.',
           'Lỗi từng game: [report.md](report.md). Nhiều game đã vượt điểm chặn cũ nhưng có lỗi tiếp theo.',
           'Ví dụ: AoE Asian Dynasties vượt toHexString/currentThread/monitorenter, còn thiếu Command; Boom-Ola/Sơn Tinh vượt monitorenter, còn thiếu Form; Dragonhunter vượt getBytes(encoding), còn thiếu writeLong.',
           'Assassin’s Creed desktop: '+str(next(x['host'].get('draws') for x in files if Path(x['path']).name=='AssassinsCreed.jar'))+' lệnh vẽ; trước nâng cấp chưa có lệnh vẽ. So sánh này là quan sát tiến triển, không phải benchmark FPS.','',
           'API tham khảo: [Oracle Thread CLDC 1.1](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/java/lang/Thread.html), [Oracle Runtime CLDC 1.1](https://docs.oracle.com/javame/config/cldc/ref-impl/cldc1.1/jsr139/java/lang/Runtime.html).']
    (folder/'summary.md').write_text('\n'.join(lines)+'\n',encoding='utf8')
    print(json.dumps({'files':len(files),'host_no_error':len(host),'arm_no_error':len(arm),'clean_quit':cleanup,'sources_unchanged':True}))
if __name__=='__main__':main()
