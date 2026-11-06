"""Exercise the actual ARM launcher against nested mocked phone/card filesystems."""
import json
from arm_smoke import Machine, ROOT, finish_scan

def launch(filesystem, drives=None):
    m = Machine(filesystem=filesystem)
    if drives is not None:
        m.drives = drives
    m.call(m.symbol('gcc_entry'), m.resolver, 0, 0)
    m.call(m.callbacks['system'], 1, 0)
    finish_scan(m)
    return m

def paths(m):
    count = m.get32(m.symbol('count'))
    return [bytes(m.u.mem_read(m.symbol('paths')+260*i, 260)).split(b'\0')[0].decode('utf-8') for i in range(count)]

def quit(m):
    m.call(m.callbacks['system'], 5, 0)
    assert not m.finders and m.used == 0, (m.finders, m.used)

def main():
    jar = (ROOT/'demo/PocketDemo.jar').read_bytes()
    fs = {
        r'E:\Games': None, r'E:\Games\Nested': None,
        r'E:\Games\Nested\Demo.JAR': jar,
        r'E:\Games\Nested\Demo.JAD': b'MIDlet-Jar-URL: Demo.JAR\r\n',
        r'E:\Games\Nested\ignore.jar.bak': b'bad',
        r'E:\folder.jar': None, r'E:\folder.jar\game.txt': b'bad',
        r'E:\Trò chơi': None, r'E:\Trò chơi\Điện thoại.jar': jar,
        r'E:\.': None, r'E:\..': None,
        r'C:\Java': None, r'C:\Java\Demo.JAR': jar,
    }
    m = launch(fs)
    found = paths(m)
    expected = {p for p, data in fs.items() if data is not None and p.lower().endswith(('.jar', '.jad'))}
    assert set(found) == expected, found
    assert r'E:\*' in m.scan_patterns and r'C:\*' in m.scan_patterns
    # Duplicate basenames on different drives remain independently selectable.
    index = found.index(r'E:\Games\Nested\Demo.JAD')+1
    for _ in range(index): m.call(m.callbacks['key'], 2, 56)
    m.save('arm-scanner-list.png')
    m.call(m.callbacks['key'], 2, 53)
    assert m.get32(m.symbol('scan_queue')) == 0 and not m.finders
    for _ in range(12):
        m.time += 33; m.call(m.callbacks['timer'], 1)
    assert any(s == 'REAL MIDLET / BYTECODE' for _, _, s, _ in m.rendered_texts)
    assert r'E:\Games\Nested\Demo.JAR' in m.filepaths.values()
    m.call(m.callbacks['key'], 2, 42); m.call(m.callbacks['key'], 2, 48)
    fs[r'C:\New.jad'] = b'MIDlet-Jar-URL: absent.jar\n'
    m.call(m.callbacks['key'], 2, 35); finish_scan(m)
    assert r'C:\New.jad' in paths(m), paths(m)
    # UTF-8 discovery paths are converted back to UCS2 when opening a JAR.
    index = paths(m).index(r'E:\Trò chơi\Điện thoại.jar')+1
    for _ in range(index): m.call(m.callbacks['key'], 2, 56)
    m.call(m.callbacks['key'], 2, 53)
    for _ in range(12): m.time += 33; m.call(m.callbacks['timer'], 1)
    assert r'E:\Trò chơi\Điện thoại.jar' in m.filepaths.values()
    quit(m)
    m = launch({}); assert paths(m) == []; quit(m)
    m = launch({r'E:\x.jar': jar}, (ord('E'), ord('E')))
    assert m.scan_patterns.count(r'E:\*') == 1; quit(m)
    m = launch({fr'E:\game-{i:02}.jar': jar for i in range(70)})
    assert len(paths(m)) == 64 and m.get32(m.symbol('scan_limited')) == 1
    for _ in range(64): m.call(m.callbacks['key'], 2, 56)
    assert m.get32(m.symbol('selected')) == 64
    m.save('arm-scanner-scroll.png'); quit(m)
    # Queue overflow is bounded and reported, with all find handles closed.
    m = launch({fr'E:\dir{i:03}': None for i in range(110)})
    assert m.get32(m.symbol('scan_limited')) == 1; quit(m)
    deep = {}; path = 'E:'
    for depth in range(1, 9):
        path += '\\d'; deep[path] = None; deep[path+'\\game.jar'] = jar
    m = launch(deep); assert len(paths(m)) == 6 and m.get32(m.symbol('scan_limited')); quit(m)
    m = Machine(filesystem={fr'E:\x{i}.jar': jar for i in range(40)})
    m.call(m.symbol('gcc_entry'), m.resolver, 0, 0); m.call(m.callbacks['system'], 1, 0)
    m.time += 33; m.call(m.callbacks['timer'], 1)
    assert m.get32(m.symbol('scan_active')) and len(paths(m)) == 16
    assert m.finders; m.call(m.callbacks['system'], 6, 0)
    before = paths(m); m.call(m.callbacks['system'], 1, 0); finish_scan(m)
    assert len(paths(m)) == 40 and before == paths(m)[:16]; quit(m)
    m = Machine(filesystem=fs); m.call(m.symbol('gcc_entry'), m.resolver, 0, 0)
    m.call(m.callbacks['system'], 1, 0); m.call(m.callbacks['system'], 7, 0)
    assert paths(m) == [] and not m.get32(m.symbol('scan_active')); quit(m)
    # Opening a demo or quitting during discovery releases its transient queue.
    m = Machine(filesystem=fs); m.call(m.symbol('gcc_entry'), m.resolver, 0, 0)
    m.call(m.callbacks['system'], 1, 0); assert m.get32(m.symbol('scan_active'))
    m.call(m.callbacks['key'], 2, 53); assert m.get32(m.symbol('scan_queue')) == 0; quit(m)
    report = {'recursive_two_drives': 'passed', 'case_insensitive_extensions': 'passed',
              'same_basename_distinct_paths': 'passed', 'nested_jad_open': 'passed',
              'unicode_path_open': 'passed', 'rescan': 'passed', 'empty_storage': 'passed',
              '64_file_cap_and_scroll': 'passed', 'directory_queue_cap': 'passed',
              'open_during_scan_cleanup': 'passed', 'depth_limit': 'passed',
              'incremental_hide_resume': 'passed', 'card_removal_cleanup': 'passed', 'runtime': 'Actual ARM VXP; mocked MRE file APIs'}
    (ROOT/'build/scanner-test.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    main()
