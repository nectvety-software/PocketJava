"""Package the verified signed application and demo without signing keys."""
import hashlib
import json
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    target = ROOT/'dist/PocketJava-signed-package.zip'
    target.parent.mkdir(exist_ok=True)
    files = {
        'PocketJava-signed.vxp': 'build/PocketJava-signed.vxp',
        'PocketJava-signed.vxp.sha256': 'build/PocketJava-signed.vxp.sha256',
        'j2me/PocketDemo.jar': 'demo/PocketDemo.jar',
        'j2me/PocketDemo.jad': 'demo/PocketDemo.jad',
        'README.md': 'README.md',
        'reports/signing-report.json': 'build/signing-report.json',
        'reports/test-report.json': 'build/test-report.json',
        'reports/arm-smoke.json': 'build/arm-smoke.json',
        'reports/scanner-test.json': 'build/scanner-test.json',
        'reports/tone-test.json': 'build/tone-test.json',
        'reports/graphics-test.json': 'build/graphics-tests/report.json',
        'reports/gameplay-before.json': 'build/gameplay-before.json',
        'reports/gameplay-after.json': 'build/gameplay-after.json',
        'reports/gameplay-slice-test.json': 'build/gameplay-slice-test.json',
        'GAMEPLAY-PERFORMANCE.md': 'docs/GAMEPLAY-PERFORMANCE.md',
        'CLDC-MIDP-COVERAGE.md': 'docs/CLDC-MIDP-COVERAGE.md',
        'CORE-UPGRADE-2.md': 'docs/CORE-UPGRADE-2.md',
        'NOKIA225-REBOOT-FIX.md': 'docs/NOKIA225-REBOOT-FIX.md',
        'MRE-FILE-IO-FIX.md': 'docs/MRE-FILE-IO-FIX.md',
        'NINJASCHOOL3.md': 'docs/NINJASCHOOL3.md',
        'reports/image-decode.json': 'build/image-decode/report.json',
        'reports/launch-safety.json': 'build/launch-safety.json',
        'reports/ui-test.json': 'build/ui-preview/report.json',
        'reports/compat-arm-test.json': 'build/compat-arm-test.json',
        'reports/gamecanvas-test.json': 'build/gamecanvas-test.json',
    }
    # Install payload and signing evidence must exist; older measurement reports are
    # shipped only when they are still present in build/.
    required={'PocketJava-signed.vxp','PocketJava-signed.vxp.sha256','j2me/PocketDemo.jar',
              'j2me/PocketDemo.jad','README.md','reports/signing-report.json'}
    for name in required:
        assert (ROOT/files[name]).exists(), f'missing required package file: {files[name]}'
    files={name:source for name,source in files.items() if name in required or (ROOT/source).exists()}
    game_report=ROOT/'build/ninjaschool3-final/report.json'
    if game_report.exists():
        report=json.loads(game_report.read_text(encoding='utf-8'))
        digest=hashlib.sha256((ROOT/'build/PocketJava-signed.vxp').read_bytes()).hexdigest()
        if report.get('source_files_unchanged') and report.get('signed_vxp_sha256')==digest:
            for name in ('report.json','report.md','summary.json','summary.md'):
                source='build/ninjaschool3-final/'+name
                if (ROOT/source).exists():files['reports/games/'+name]=source
    scenario=ROOT/'build/ninja-scenario/report.json'
    if scenario.exists():
        report=json.loads(scenario.read_text(encoding='utf-8'))
        digest=hashlib.sha256((ROOT/'build/PocketJava-signed.vxp').read_bytes()).hexdigest()
        if report.get('source_files_unchanged') and report.get('signed_vxp_sha256')==digest:
            files['reports/ninja-scenario.json']='build/ninja-scenario/report.json'
            for stage in report['stages']:
                name=stage['name']+'.png'
                files['reports/ninja-scenario/'+name]='build/ninja-scenario/'+name
    compat_report=ROOT/'build/upgrade2-final-20261005/report.json'
    if compat_report.exists():
        report=json.loads(compat_report.read_text(encoding='utf-8'))
        digest=hashlib.sha256((ROOT/'build/PocketJava-signed.vxp').read_bytes()).hexdigest()
        if report.get('source_files_unchanged') and report.get('signed_vxp_sha256')==digest:
            for name in ('report.json','report.md','summary.md','summary.json','api-inventory.json','api-inventory.md','moto-timed.json'):
                source='build/upgrade2-final-20261005/'+name
                if (ROOT/source).exists():files['reports/compatibility/'+name]=source
    guide = (
        'PocketJava — JAR/JAD trên MRE\n\n'
        'Chép PocketJava-signed.vxp vào máy. Đặt JAR/JAD trên thẻ hoặc bộ nhớ máy, '
        'kể cả thư mục con. JAD trỏ tên JAR hoặc đường dẫn tương đối vào thư mục con.\n'
        'Tự quét cả hai ổ; 2/8 chọn, 5 mở, # quét lại, * tạm dừng game.\n'
        'Tối đa 64 tệp, sâu 6 cấp; báo khi chạm giới hạn.\n'
        'Bản này sửa đọc ZIP qua MRE: seek dùng BASE_BEGIN, đọc theo khối 1 KiB.\n'
        'Bản ký: Cert ID 100, IMSI *, OpenSSL VERIFY OK.\n'
        'Chưa xác nhận gameplay đầy đủ hay FPS/firmware Nokia thật.\n'
        'Có tone MIDI MRE; Player nhạc/SFX từ stream vẫn chỉ mô phỏng.\n'
        'Không chứa khóa ký. Xem README.md và reports.\n'
    )
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED) as archive:
        for name, source in files.items():
            archive.write(ROOT/source, name)
        archive.writestr('HUONG-DAN.txt', guide.encode('utf-8'))
    with zipfile.ZipFile(target) as archive:
        assert archive.testzip() is None
        assert archive.read('PocketJava-signed.vxp') == (ROOT/files['PocketJava-signed.vxp']).read_bytes()
        assert not any(name.lower().endswith('.pem') for name in archive.namelist())
    target.with_suffix('.zip.sha256').write_bytes(
        (hashlib.sha256(target.read_bytes()).hexdigest()+'  '+target.name+'\n').encode('ascii'))
    print(f'Packaged {target} ({target.stat().st_size} bytes)')

if __name__ == '__main__':
    main()
