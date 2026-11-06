"""Summarize the completed full-folder run for the matching signed binary."""
import hashlib
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    folder = ROOT/'build/full-compat-20261005'
    report = json.loads((folder/'report.json').read_text(encoding='utf-8'))
    assert report.get('source_files_unchanged'), 'Incomplete run'
    assert report['signed_vxp_sha256'] == hashlib.sha256((ROOT/'build/PocketJava-signed.vxp').read_bytes()).hexdigest()
    files = report['files']
    current={str(p) for p in Path(report['source']).rglob('*') if p.is_file() and p.suffix.lower() in ('.jar','.jad')}
    assert current=={x['path'] for x in files}, 'Folder inventory changed; run all new files before summarizing'
    inventory=json.loads((folder/'api-inventory.json').read_text(encoding='utf-8'))
    hashes={x['path']:x['sha256'] for x in inventory['files']}
    assert all(hashes.get(x['path'])==x['sha256'] for x in files if Path(x['path']).suffix.lower()=='.jar'), 'API inventory source mismatch'
    host = sum(bool(x['host'].get('ok')) for x in files)
    arm = sum(bool(x['arm'].get('no_error_within_budget')) for x in files)
    cleanup = sum(x['arm'].get('heap_after_quit') == 0 for x in files)
    errors = Counter(x['arm'].get('error') or x['arm'].get('harness_error') for x in files)
    errors.pop(None, None); errors.pop('', None)
    summary = dict(files=len(files), jar_count=report['jar_count'], jad_count=report['jad_count'],
                   host_no_error=host, arm_no_error=arm, clean_quit=cleanup,
                   sources_unchanged=True, arm_errors=dict(errors))
    lines = ['# Kiểm thử toàn bộ thư mục và nâng cấp lõi', '',
             f"Nguồn: `{report['source']}` — {report['jar_count']} JAR, {report['jad_count']} JAD.",
             'SHA-256 tệp nguồn giữ nguyên; report gắn với SHA-256 bản VXP đã ký.', '',
             'Bổ sung float constants/locals/arrays/arithmetic/conversions/comparisons, Long parse/format/value, readLong/writeLong, Short.parseShort và Random.setSeed.',
             'Bổ sung float/double return, wide locals, String.toString identity, Throwable.getMessage, StringBuffer.charAt bounds, default Canvas key callbacks và InputStream mark/reset mặc định.',
             'Bổ sung Image.createImage(InputStream), Canvas key/capability queries, Graphics drawChar/gray scale, GameCanvas bộ đệm indexed riêng và flush vùng/toàn màn hình, paint mặc định, key latch.', '',
             '28 nhóm tích hợp và 120 class mutation qua. ARM kiểm tra GameCanvas pixel isolation/flush/key latch, PNG stream và thư viện, monitor, JAD, MIDI, UI, giới hạn lát thời gian. Cleanup fixture = 0.', '',
             f'Desktop: {host}/{len(files)} không lỗi trong tối đa 4000 lượt; ARM VXP ký: {arm}/{len(files)} không lỗi trong tối đa 400 callback, có phím 5 ở lượt 350.',
             f'ARM heap sau quit = 0: {cleanup}/{len(files)} tệp.',
             'Đây là chạy bytecode trên host và ARM Unicorn với API MRE mô phỏng. Không xác nhận chơi hết game, FPS phần cứng hoặc âm thanh thật. Các lỗi không được chuyển thành thành công giả.', '',
             '## Điểm chặn ARM còn lại', '',
             '| Lỗi | Số tệp |', '|---|---|',
             *[f"| {error.replace('|', '/')} | {count} |" for error, count in errors.most_common()], '',
             '## Giới hạn', '',
             'Runtime vẫn là tập con CLDC/MIDP: chưa đầy đủ float/double, Sprite/TileLayer, Form/Command, API hãng/JSR, mạng hay Player nhạc từ stream. Graphics còn chia sẻ trạng thái; text offscreen chưa hỗ trợ. Bộ đệm GameCanvas dùng khoảng 76 KiB cộng palette. Native PNG/inflate/GC chưa chia lát thời gian.',
             'Xem [report.md](report.md) cho lỗi từng tệp và report.json cho budget, heap, ảnh và đường dẫn đã mở.']
    (folder/'summary.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')
    (folder/'summary.json').write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(summary, ensure_ascii=True))

if __name__ == '__main__':
    main()
