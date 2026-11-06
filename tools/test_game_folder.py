"""Read-only compatibility checks of real JAR/JAD files on host and signed ARM VXP."""
import argparse
import hashlib
import json
import os
import re
import subprocess
import time
import zipfile
from concurrent.futures import ProcessPoolExecutor, as_completed
from datetime import datetime, timezone
from pathlib import Path
from arm_smoke import Machine, ROOT, finish_scan

OUT = ROOT/'build/gamejava-test'
SIGNED = ROOT/'build/PocketJava-signed.vxp'

def inspect(path):
    result = {'path': str(path), 'bytes': path.stat().st_size,
              'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    if path.suffix.lower() == '.jar':
        try:
            with zipfile.ZipFile(path) as archive:
                manifest = archive.read('META-INF/MANIFEST.MF').decode('utf-8', errors='replace')
                props = {}
                for line in manifest.replace('\r', '').split('\n'):
                    if ':' in line:
                        key, value = line.split(':', 1); props[key] = value.strip()
                result['manifest'] = {k: v for k, v in props.items() if k.startswith(('MIDlet-', 'MicroEdition-')) or k == 'Main-Class'}
                result['has_midlet'] = 'MIDlet-1' in props
                result['class_count'] = sum(p.filename.endswith('.class') for p in archive.infolist())
        except (OSError, KeyError, zipfile.BadZipFile) as error:
            result['inspection_error'] = str(error)
    return result

def host_test(path, directory, steps):
    env = dict(os.environ)
    env['PATH'] = r'C:\msys64\mingw64\bin'+os.pathsep+env.get('PATH', '')
    before = time.monotonic()
    try:
        process = subprocess.run([str(ROOT/'build/pocketjava.exe'), str(path), str(steps),
                                  str(directory/'host.ppm'), '350:53,650:53,950:53'],
                                 cwd=ROOT, env=env, capture_output=True, timeout=60)
        (directory/'host.log').write_bytes(process.stdout+b'\nSTDERR\n'+process.stderr)
        output = process.stdout.decode('utf-8', errors='replace')
        match = re.search(r'RESULT ok=(\d+) instructions=(\d+) draws=(\d+) memory=(\d+) peak=(\d+) error=(.*)', output)
        result = {'exit_code': process.returncode, 'wall_seconds': round(time.monotonic()-before, 3), 'steps_limit': steps}
        if match:
            values = match.groups()
            result.update(dict(zip(('ok', 'instructions', 'draws', 'memory', 'peak'), map(int, values[:5]))))
            result['error'] = values[5].strip()
        result['memory_leak_reported'] = b'Memory leak' in process.stderr
        return result
    except subprocess.TimeoutExpired:
        return {'error': 'Host test timed out after 60 seconds', 'steps_limit': steps}

def arm_test(path, directory, steps):
    machine = None
    result = {'steps_limit': steps, 'gameplay_verified': False}
    before = time.monotonic()
    image_folder = directory.relative_to(ROOT/'build').as_posix()
    try:
        machine = Machine(external_jar=path, vxp_path=SIGNED)
        machine.call(machine.symbol('gcc_entry'), machine.resolver, 0, 0)
        machine.call(machine.callbacks['system'], 1, 0); finish_scan(machine)
        # Discover and open this actual archive through the launcher's MRE file APIs.
        assert machine.get32(machine.symbol('count')) == 1
        machine.call(machine.callbacks['key'], 2, 56)
        machine.call(machine.callbacks['key'], 2, 53)
        error = machine.cstr(machine.symbol('status'))
        callbacks = 0
        if not error:
            for step in range(steps):
                if step in (350, 650, 950): machine.call(machine.callbacks['key'], 2, 53)
                machine.time += 33; machine.call(machine.callbacks['timer'], 1)
                callbacks += 1
                error = machine.cstr(machine.symbol('status'))
                if step in (349, 649, 949): machine.save(f'{image_folder}/arm-{step}.png')
                if step % 100 == 0: print('ARM', path.name, 'callback', step, 'heap', machine.used, flush=True)
                if error: break
        result.update({'callbacks': callbacks, 'error': error,
                       'no_error_within_budget': not bool(error),
                       'heap_peak_including_framebuffer': machine.peak,
                       'last_text': [s for _, _, s, _ in machine.rendered_texts],
                       'opened_paths': list(machine.filepaths.values())})
        machine.save(f'{image_folder}/arm-result.png')
    except Exception as error:
        result['harness_error'] = f'{type(error).__name__}: {error}'
    finally:
        if machine:
            try:
                machine.call(machine.callbacks['system'], 5, 0)
                result['heap_after_quit'] = machine.used
            except Exception as error:
                result['cleanup_error'] = str(error)
        result['wall_seconds'] = round(time.monotonic()-before, 3)
    return result

def write_report(report):
    temporary = OUT/'report.json.tmp'
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    temporary.replace(OUT/'report.json')
    lines = ['# Kiểm thử thư mục game Java', '', f"Nguồn: `{report['source']}`", '',
             'Thực thi bản VXP đã ký trên ARM Unicorn với API MRE mô phỏng; không phải đo FPS điện thoại.',
             'Không lỗi trong số lượt thử không đồng nghĩa chơi hoàn chỉnh. Không sửa tệp nguồn.', '',
             '| Tệp | Desktop | ARM VXP đã ký |', '|---|---|---|']
    for item in report['files']:
        host = item.get('host', {})
        arm = item.get('arm', {})
        hs = host.get('error') or ('Không lỗi trong lượt thử' if host.get('ok') else 'Chưa chạy')
        ars = arm.get('error') or arm.get('harness_error') or ('Không lỗi trong lượt thử; chưa xác nhận gameplay' if arm.get('no_error_within_budget') else 'Chưa chạy')
        lines.append(f"| {Path(item['path']).name} | {hs.replace('|', '/')} | {ars.replace('|', '/')} |")
    (OUT/'report.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')

def main():
    global OUT
    parser = argparse.ArgumentParser()
    parser.add_argument('folder', type=Path)
    parser.add_argument('--output', type=Path, default=OUT)
    parser.add_argument('--only', action='append', default=[])
    parser.add_argument('--host-steps', type=int, default=4000)
    parser.add_argument('--arm-steps', type=int, default=1200)
    parser.add_argument('--resume', action='store_true', help='Continue a matching binary/source/budget snapshot')
    parser.add_argument('--jobs', type=int, default=1, choices=(1, 2, 3, 4), help='Independent ARM test processes')
    args = parser.parse_args()
    OUT = args.output.resolve()
    OUT.mkdir(parents=True, exist_ok=True)
    files = sorted((p for p in args.folder.rglob('*') if p.is_file() and p.suffix.lower() in ('.jar', '.jad')), key=lambda p: p.name.lower())
    if args.only:files=[p for p in files if p.name in args.only]
    report = {'source': str(args.folder), 'utc': datetime.now(timezone.utc).isoformat(),
              'signed_vxp': str(SIGNED), 'signed_vxp_sha256': hashlib.sha256(SIGNED.read_bytes()).hexdigest(),
              'jar_count': sum(p.suffix.lower() == '.jar' for p in files),
              'jad_count': sum(p.suffix.lower() == '.jad' for p in files), 'files': []}
    previous = {}
    if args.resume and (OUT/'report.json').exists():
        old = json.loads((OUT/'report.json').read_text(encoding='utf-8'))
        assert old['signed_vxp_sha256'] == report['signed_vxp_sha256'], 'Resume binary changed'
        previous = {item['path']: item for item in old['files']}
    for number, path in enumerate(files):
        directory = OUT/f'{number+1:02}'
        directory.mkdir(exist_ok=True)
        item = inspect(path); item['artifacts'] = str(directory)
        cached = previous.get(str(path))
        if cached and cached.get('artifacts') == str(directory) and cached['sha256'] == item['sha256'] and cached.get('host', {}).get('steps_limit') == args.host_steps:
            item['host'] = cached['host']
            if cached.get('arm', {}).get('steps_limit') == args.arm_steps:
                item['arm'] = cached['arm']
            report['files'].append(item); write_report(report)
            continue
        item['host'] = host_test(path, directory, args.host_steps)
        report['files'].append(item); write_report(report)
        print('HOST', path.name, json.dumps(item['host'], ensure_ascii=True), flush=True)
    pending = [(number, path) for number, path in enumerate(files) if 'arm' not in report['files'][number]]
    if args.jobs == 1:
        for number, path in pending:
            item = report['files'][number]
            item['arm'] = arm_test(path, Path(item['artifacts']), args.arm_steps)
            write_report(report)
            print('RESULT ARM', path.name, json.dumps(item['arm'], ensure_ascii=True), flush=True)
    else:
        with ProcessPoolExecutor(max_workers=args.jobs) as pool:
            futures = {pool.submit(arm_test, path, Path(report['files'][number]['artifacts']), args.arm_steps): number for number, path in pending}
            for future in as_completed(futures):
                number = futures[future]; item = report['files'][number]
                item['arm'] = future.result()
                write_report(report)
                print('RESULT ARM', files[number].name, json.dumps(item['arm'], ensure_ascii=True), flush=True)
    for item in report['files']:
        assert hashlib.sha256(Path(item['path']).read_bytes()).hexdigest() == item['sha256'], 'Source archive changed'
    report['source_files_unchanged'] = True; write_report(report)
    print('COMPLETE', OUT/'report.md', flush=True)

if __name__ == '__main__':
    main()
