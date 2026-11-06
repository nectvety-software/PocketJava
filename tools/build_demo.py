"""Compile a genuine MIDlet, excluding compile-only API stubs from its JAR."""
import pathlib, subprocess, zipfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
def build():
    out=ROOT/'build/demo-classes'; out.mkdir(parents=True,exist_ok=True)
    sources=list((ROOT/'demo/api').rglob('*.java'))+list((ROOT/'demo/java').rglob('*.java'))
    subprocess.run(['javac','--release','8','-Xlint:-options','-d',str(out),*[str(p) for p in sources]],check=True)
    manifest=('Manifest-Version: 1.0\r\nMIDlet-Name: PocketJava Demo\r\nMIDlet-Version: 0.1.0\r\nMIDlet-Vendor: PocketJava\r\nMIDlet-1: PocketJava Demo,,PocketDemo\r\nMicroEdition-Configuration: CLDC-1.0\r\nMicroEdition-Profile: MIDP-2.0\r\n\r\n')
    target=ROOT/'demo/PocketDemo.jar'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED) as z:
        for name,data in [('META-INF/MANIFEST.MF',manifest.encode('ascii'))]+[(p.name,p.read_bytes()) for p in sorted(out.glob('*.class'))]:
            info=zipfile.ZipInfo(name,date_time=(2026,1,1,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED
            z.writestr(info,data)
    (ROOT/'demo/PocketDemo.jad').write_text(manifest+f'MIDlet-Jar-URL: PocketDemo.jar\nMIDlet-Jar-Size: {target.stat().st_size}\n',encoding='ascii')
    data=target.read_bytes()
    (ROOT/'native/demo_jar.h').write_text('/* Generated from demo/PocketDemo.jar by build_demo.py */\nstatic const unsigned char demo_jar[]={\n'+',\n'.join(','.join(str(b) for b in data[i:i+24]) for i in range(0,len(data),24))+'\n};\n',encoding='ascii')
    print(f'Built {target} ({len(data)} bytes)')
if __name__=='__main__':build()
