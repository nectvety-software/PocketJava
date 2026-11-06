"""Integration checks: genuine classes, GC, archive errors, JAD, ARM and packaging."""
import json,os,pathlib,random,subprocess,time,zipfile,sys
from io import BytesIO
from PIL import Image
ROOT=pathlib.Path(__file__).resolve().parents[1]
OUT=ROOT/'build/tests';OUT.mkdir(parents=True,exist_ok=True)
os.environ['PATH']=r'C:\msys64\mingw64\bin'+os.pathsep+os.environ.get('PATH','')
EXE=ROOT/'build/pocketjava.exe';results=[]
def run(name,path,expect=True,contains=None,steps=80):
    t=time.monotonic();p=subprocess.run([str(EXE),str(path),str(steps)],cwd=ROOT,capture_output=True,text=True,timeout=15)
    assert (p.returncode==0)==expect,(name,p.returncode,p.stdout,p.stderr)
    assert 'Memory leak' not in p.stderr,(name,p.stderr)
    if contains:assert contains in p.stdout,(name,p.stdout[-500:])
    results.append({'test':name,'passed':True,'seconds':round(time.monotonic()-t,3),'result':p.stdout.splitlines()[-1] if p.stdout else p.stderr})
    return p
def jar(name,entries,compression=zipfile.ZIP_DEFLATED):
    target=OUT/name
    with zipfile.ZipFile(target,'w',compression) as z:
        for entry,data in entries.items():z.writestr(entry,data)
    return target
def main():
    boundary=OUT/'error-boundary.exe'
    subprocess.run([r'C:\msys64\mingw64\bin\gcc.exe','-std=c99','-O2','-Inative','-Ivendor/miniz','-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-DMINIZ_NO_MALLOC','-DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0','native/memory.c','native/jar.c','native/vm.c','vendor/miniz/miniz_tinfl.c','tests/ErrorBoundary.c','-o',str(boundary)],check=True,cwd=ROOT)
    check_boundary=subprocess.run([str(boundary)],cwd=ROOT,capture_output=True,text=True,check=True)
    assert 'ERROR BOUNDARY PASS' in check_boundary.stdout
    results.append({'test':'ZIP error buffer preserves adjacent VM title','passed':True})
    demo=ROOT/'demo/PocketDemo.jar';run('compressed JAR',demo,contains='REAL MIDLET / BYTECODE');run('JAD relative JAR',ROOT/'demo/PocketDemo.jad')
    with zipfile.ZipFile(demo) as z:entries={n:z.read(n) for n in z.namelist()}
    run('stored JAR',jar('stored.jar',entries,zipfile.ZIP_STORED))
    sub=OUT/'sub';sub.mkdir(exist_ok=True);(sub/'demo game..jar').write_bytes(demo.read_bytes())
    relative=OUT/'relative.jad';relative.write_text('MIDlet-Jar-URL: sub/demo%20game..jar\n')
    run('JAD subdirectory / percent-encoded space / dots in filename',relative)
    for url in ('sub/%2e%2e/stored.jar','%2fstored.jar','sub/%00.jar','sub/%ZZ.jar'):
        relative.write_text('MIDlet-Jar-URL: '+url+'\n');run('invalid JAD URL '+url,relative,False)
    descriptor=OUT/'size.JaD'
    descriptor.write_text(f'MIDlet-Jar-URL: stored.jar\nMIDlet-Jar-Size: {(OUT/"stored.jar").stat().st_size}\n')
    run('mixed-case JAD / matching size',descriptor)
    descriptor.write_text('MIDlet-Jar-URL: stored.jar\nMIDlet-Jar-Size: 1\n')
    run('JAD size mismatch',descriptor,False,contains='MIDlet-Jar-Size mismatch')
    for size in ('-1','42949672960','x',''):
        descriptor.write_text(f'MIDlet-Jar-URL: stored.jar\nMIDlet-Jar-Size: {size}\n')
        run('invalid JAD size '+repr(size),descriptor,False,contains='Invalid MIDlet-Jar-Size')
    api=list((ROOT/'demo/api').rglob('*.java'));classes=OUT/'classes';classes.mkdir(exist_ok=True)
    subprocess.run(['javac','--release','8','-Xlint:-options','-d',str(classes),*[str(x) for x in api],str(ROOT/'tests/VerifyMIDlet.java')],check=True)
    manifest=b'Manifest-Version: 1.0\r\nMIDlet-Name: Runtime verification\r\nMIDlet-1: Verify,,VerifyMIDlet\r\n\r\n'
    verify=jar('verify.jar',{'META-INF/MANIFEST.MF':manifest,**{p.name:p.read_bytes() for p in classes.glob('*.class')}})
    run('GC 12000 allocations / inheritance / recursion / Java arithmetic',verify,contains='SEMANTICS PASS',steps=200)
    numeric=OUT/'LibraryWide.java'
    methods=[]
    for kind, count in (('float',270),('double',137),('long',137)):
        variables=''.join(f'{kind} v{i}=seed+{i};' for i in range(count))
        total='+'.join(f'v{i}' for i in range(count))
        methods.append(f'static {kind} {kind}Sum({kind} seed){{{variables}return {total};}}')
    numeric.write_text('class LibraryWide {'+''.join(methods)+'}',encoding='ascii')
    subprocess.run(['javac','--release','8','-Xlint:-options','-d',str(classes),*[str(x) for x in api],str(numeric),str(ROOT/'tests/LibraryMIDlet.java')],check=True)
    png=BytesIO();Image.new('RGBA',(2,1),(255,0,0,255)).save(png,format='PNG')
    tall=BytesIO();Image.new('RGBA',(11,1885),(255,0,0,255)).save(tall,format='PNG')
    library=jar('library.jar',{'META-INF/MANIFEST.MF':b'MIDlet-1: Library,,LibraryMIDlet\r\n','stream.png':png.getvalue(),'tall-font.png':tall.getvalue(),'eof.bin':bytes([10,20,30,40,50]),**{p.name:p.read_bytes() for p in classes.glob('Library*.class')}})
    run('long / threads / multidimensional arrays / Unicode / streams / collections / exceptions',library,contains='LIBRARY PASS',steps=300)
    subprocess.run(['javac','--release','8','-Xlint:-options','-d',str(classes),*[str(x) for x in api],str(ROOT/'tests/MonitorMIDlet.java')],check=True)
    monitors=jar('monitors.jar',{'META-INF/MANIFEST.MF':b'MIDlet-1: Monitors,,MonitorMIDlet\n',**{p.name:p.read_bytes() for p in classes.glob('*.class') if p.stem in ('MonitorMIDlet','MonitorCanvas','Holder','Waiter')}})
    run('monitor reentrancy / contention / sleep / exception cleanup / null',monitors,contains='MONITORS PASS',steps=100)
    deep=OUT/'DeepBase.java'
    deep.write_text('class DeepBase extends javax.microedition.midlet.MIDlet {protected void startApp(){} protected void pauseApp(){} protected void destroyApp(boolean b){}}'+''.join(f'class Deep{i} extends '+('DeepBase' if i==1 else f'Deep{i-1}')+' {}' for i in range(1,13)),encoding='ascii')
    subprocess.run(['javac','--release','8','-Xlint:-options','-d',str(classes),*[str(x) for x in api],str(deep)],check=True)
    deepjar=jar('deep.jar',{'META-INF/MANIFEST.MF':b'MIDlet-1: Deep,,Deep12\n',**{p.name:p.read_bytes() for p in classes.glob('Deep*.class')}})
    run('bounded native class inheritance stack',deepjar,False,contains='Class inheritance depth limit')
    subprocess.run(['javac','--release','8','-Xlint:-options','-d',str(classes),*[str(x) for x in api],str(ROOT/'tests/GameCanvasMIDlet.java')],check=True)
    jar('gamecanvas.jar',{'META-INF/MANIFEST.MF':b'MIDlet-1: Buffered,,GameCanvasMIDlet\n',**{p.name:p.read_bytes() for p in classes.glob('*.class') if p.stem in ('GameCanvasMIDlet','BufferedGame','DefaultBufferedGame')}})
    truncated=OUT/'truncated.jar';truncated.write_bytes(demo.read_bytes()[:-8]);run('truncated ZIP rejected',truncated,False)
    corrupt=bytearray((OUT/'stored.jar').read_bytes());off=corrupt.find(b'Manifest-Version');corrupt[off]^=1
    path=OUT/'corrupt.jar';path.write_bytes(corrupt);run('CRC failure rejected',path,False,contains='CRC32')
    oversized=OUT/'oversized.jar';oversized.write_bytes(b'X'*(8*1024*1024+1));run('oversized archive rejected',oversized,False)
    bomb=jar('bomb.jar',{'META-INF/MANIFEST.MF':b'A'*(320*1024+1)});run('oversized decompressed member rejected',bomb,False)
    path=OUT/'remote.jad';path.write_text('MIDlet-Jar-URL: https://example.com/game.jar\n');run('remote JAD rejected explicitly',path,False,contains='local JAR filename')
    path=OUT/'traversal.jad';path.write_text('MIDlet-Jar-URL: ../game.jar\n');run('JAD traversal rejected',path,False)
    # Deterministic corrupted class containers with correct ZIP checksums.
    rng=random.Random(225);fuzz=OUT/'fuzz.jar';accepted=0;started=time.monotonic()
    for i in range(120):
        mutated=dict(entries);cls=bytearray(mutated['PocketDemo.class'])
        for _ in range(rng.randrange(1,5)):pos=rng.randrange(len(cls));cls[pos]^=rng.randrange(1,256)
        mutated['PocketDemo.class']=bytes(cls);jar('fuzz.jar',mutated)
        p=subprocess.run([str(EXE),str(fuzz),'3'],cwd=ROOT,capture_output=True,text=True,timeout=10)
        assert p.returncode in (0,1),(i,p.returncode,p.stdout,p.stderr)
        assert 'Memory leak' not in p.stderr,(i,p.stderr)
        accepted+=p.returncode==0
    results.append({'test':'120 deterministic class mutations / bounded execution / no crash or leak','passed':True,'accepted':accepted,'seconds':round(time.monotonic()-started,3)})
    subprocess.run([sys.executable,str(ROOT/'tools/arm_smoke.py')],check=True,cwd=ROOT)
    results.append({'test':'built ARM VXP callbacks / embedded and external JAR / lifecycle','passed':True})
    subprocess.run([sys.executable,str(ROOT/'tools/test_scanner.py')],check=True,cwd=ROOT)
    results.append({'test':'ARM whole-storage JAR/JAD scanner / recursive paths / rescan / bounded lifecycle','passed':True})
    subprocess.run([sys.executable,str(ROOT/'tools/test_graphics.py')],check=True,cwd=ROOT)
    results.append({'test':'drawRegion eight transforms / pixel oracle / clipping / Nokia FullCanvas','passed':True})
    subprocess.run([sys.executable,str(ROOT/'tools/test_image_decode.py')],check=True,cwd=ROOT)
    results.append({'test':'GIF LZW / interlace / transparency / real Ninja font strip pixel oracle / 60 truncated images','passed':True})
    subprocess.run([sys.executable,str(ROOT/'tools/test_tone.py')],check=True,cwd=ROOT)
    results.append({'test':'ARM MIDI tone payload / pause stop / cleanup','passed':True})
    (ROOT/'build/test-report.json').write_text(json.dumps(results,indent=2),encoding='utf8')
    print(f'{len(results)} integration groups passed; 120 mutation cases. Report: build/test-report.json')
if __name__=='__main__':main()
