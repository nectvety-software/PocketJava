"""Summarize this bounded compatibility run without implying verified gameplay."""
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def main():
    folder=ROOT/'build/session-20261004-final'
    report=json.loads((folder/'report.json').read_text(encoding='utf-8'))
    assert report.get('source_files_unchanged'), 'Game test run is incomplete'
    baseline=json.loads((ROOT/'build/session-20261004/report.json').read_text(encoding='utf-8'))
    old={Path(x['path']).name:x['host'] for x in baseline['files']}
    files=report['files'];host=[Path(x['path']).name for x in files if x['host'].get('ok')]
    arm=[Path(x['path']).name for x in files if x['arm'].get('no_error_within_budget')]
    changed=[]
    for x in files:
        name=Path(x['path']).name;before=old[name].get('error');after=x['host'].get('error')
        if before!=after:changed.append({'game':name,'before':before,'after':after})
    summary={'jar_count':report['jar_count'],'jad_count':report['jad_count'],'host_steps':4000,'arm_callbacks':120,
             'host_no_error':host,'arm_no_error':arm,'host_blockers_changed':changed,
             'host_no_error_but_no_draws':[Path(x['path']).name for x in files if x['host'].get('ok') and not x['host'].get('draws')],
             'arm_heap_peak_including_framebuffer':max(x['arm'].get('heap_peak_including_framebuffer',0) for x in files),
             'arm_clean_quit':sum(x['arm'].get('heap_after_quit')==0 for x in files),
             'source_files_unchanged':True,'gameplay_verified':False}
    (folder/'summary.json').write_text(json.dumps(summary,ensure_ascii=False,indent=2),encoding='utf-8')
    lines=['# Cập nhật lõi và kiểm thử ngày 04/10/2026','',
           f"Nguồn: `{report['source']}`. {report['jar_count']} JAR, {report['jad_count']} JAD; SHA-256 tệp nguồn giữ nguyên.",'',
           'Đã bổ sung Integer hex/octal/binary, DataInputStream.skipBytes với scratch 64 byte, Nokia FullCanvas, kiểm tra kích thước và đuôi JAD trộn hoa/thường.',
           'Đồ họa cắt phần ảnh nằm ngoài clip/đích trước khi giải mã; test pixel cả 8 phép biến đổi đã qua.',
           'Manager.playTone tạo MIDI trong buffer tĩnh 48 byte và gọi API MRE. Một tone cùng lúc; pause/hủy VM dừng tone. Stream Player nhạc/SFX vẫn mô phỏng, chưa phát âm thanh từ JAR.',
           '22 nhóm test tích hợp và 120 class mutation đã qua, gồm JAD, GC, scanner, ảnh và MIDI ARM.','',
           f'Desktop: {len(host)}/{len(files)} không lỗi trong 4.000 lượt. ARM VXP đã ký: {len(arm)}/{len(files)} không lỗi trong 120 callback.',
           'Các budget khác nhau, không so trực tiếp tỷ lệ desktop/ARM. ARM dùng Unicorn và API MRE mô phỏng; không đo FPS hay xác nhận firmware/âm thanh điện thoại.',
           'Assassin’s Creed hết budget desktop không lỗi nhưng draws = 0; không coi đây là bằng chứng game hiển thị được.',
           'Host có phím 5 tại lượt 350/650/950; lượt ARM 120 callback chưa đến các mốc phím này. Chưa xác nhận gameplay đầy đủ.',
           f"Peak ARM gồm framebuffer: {summary['arm_heap_peak_including_framebuffer']} byte; heap sau quit = 0 ở {summary['arm_clean_quit']}/{len(files)} tệp. Heap không gồm toàn bộ code/stack/BSS/firmware.",'',
           '## Game không lỗi trong budget ARM','',*['- '+name for name in arm],'',
           '## Điểm chặn desktop đã thay đổi','',
           '| Game | Trước | Sau |','|---|---|---|',
           *[f"| {x['game']} | {x['before']} | {x['after'] or 'Không lỗi trong budget'} |" for x in changed],'',
           'Chi tiết lỗi từng game: [report.md](report.md). Tệp VXP: `build/PocketJava-signed.vxp`, gói phân phối: `dist/PocketJava-signed-package.zip`.']
    (folder/'summary.md').write_text('\n'.join(lines)+'\n',encoding='utf-8');print(json.dumps(summary,ensure_ascii=True,indent=2))
if __name__=='__main__':main()
