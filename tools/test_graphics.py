"""Pixel oracle for clipped drawRegion, all transforms, and Nokia FullCanvas."""
import os, subprocess, zipfile, json
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
def main():
    out=ROOT/'build/graphics-tests';out.mkdir(exist_ok=True)
    classes=out/'classes';classes.mkdir(exist_ok=True)
    subprocess.run(['javac','--release','8','-d',str(classes),*[str(p) for p in (ROOT/'demo/api').rglob('*.java')],str(ROOT/'tests/GraphicsMIDlet.java')],check=True)
    jar=out/'graphics.jar'
    with zipfile.ZipFile(jar,'w',zipfile.ZIP_DEFLATED) as z:
        z.writestr('META-INF/MANIFEST.MF','MIDlet-1: Graphics,,GraphicsMIDlet\n')
        for p in classes.glob('*.class'):z.write(p,p.name)
    env=dict(os.environ);env['PATH']=r'C:\msys64\mingw64\bin'+os.pathsep+env.get('PATH','')
    result=subprocess.run([str(ROOT/'build/pocketjava.exe'),str(jar),'20',str(out/'result.ppm')],env=env,capture_output=True,text=True,check=True)
    actual=Image.open(out/'result.ppm');expected=Image.new('RGB',(240,320),(128,128,128))
    colors=[(255,0,0),(0,255,0),(0,0,255),(255,255,255),(0,0,0),(255,255,0)]
    for t in range(8):
        w,h=(2,3) if t>=4 else (3,2)
        for y in range(h):
            for x in range(w):
                xy=[(x,y),(x,1-y),(2-x,y),(2-x,1-y),(y,x),(y,1-x),(2-y,x),(2-y,1-x)][t]
                expected.putpixel((10+t*10+x,10+y),colors[xy[1]*3+xy[0]])
    expected.putpixel((1,1),colors[4]);expected.putpixel((2,1),colors[5])
    assert actual.tobytes()==expected.tobytes(),'drawRegion pixels differ from the oracle'
    assert 'Memory leak' not in result.stderr
    actual.save(out/'result.png')
    report={'transforms':8,'clipping':'passed','fullcanvas':'passed','pixel_oracle':'passed'}
    (out/'report.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':main()
