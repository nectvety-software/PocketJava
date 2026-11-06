"""PNG/GIF pixel oracle and bounded malformed GIF checks."""
import json,os,subprocess,zipfile,random
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'build/image-decode';out.mkdir(exist_ok=True)
env=dict(os.environ);env['PATH']=r'C:\msys64\mingw64\bin'+os.pathsep+env.get('PATH','')
exe=out/'decode.exe'
subprocess.run([r'C:\msys64\mingw64\bin\gcc.exe','-std=c99','-O2','-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-DMINIZ_NO_MALLOC','-DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0','-DNDEBUG','-Inative','-Ivendor/miniz','tests/ImageDecode.c','native/memory.c','native/jar.c','vendor/miniz/miniz_tinfl.c','-o',str(exe)],cwd=ROOT,env=env,check=True)
def check(path):
    subprocess.run([str(exe),str(path),str(out/'result.ppm')],cwd=ROOT,env=env,check=True,capture_output=True,timeout=10)
    source=Image.open(path).convert('RGBA');expected=[]
    for r,g,b,a in source.getdata():expected.append((128,128,128) if a<128 else ((r>>3)*255//31,(g>>2)*255//63,(b>>3)*255//31))
    actual=Image.open(out/'result.ppm');assert list(actual.getdata())==expected,path
    return {'file':path.name,'dimensions':source.size,'pixel_oracle':'passed'}
results=[]
for name,size in [('small',(3,2)),('interlaced',(31,49)),('dictionary',(240,320))]:
    image=Image.new('P',size);image.putpalette([component for k in range(256) for component in ((k*37)%256,(k*53)%256,(k*71)%256)])
    rng=random.Random(741);image.putdata([rng.randrange(256) for _ in range(size[0]*size[1])])
    path=out/(name+'.gif');image.save(path,transparency=3,interlace=True,optimize=False);results.append(check(path))
game=Path('D:/Program/gamejava/j2me/NinjaSchool3_240x320.jar')
with zipfile.ZipFile(game) as z:
    for name in ('u/tt.png','fb.png','fr.png'):
        path=out/('ninja-'+name.replace('/','-'));path.write_bytes(z.read(name));results.append(check(path))
data=(out/'dictionary.gif').read_bytes();rng=random.Random(41);malformed=0
for i in range(60):
    path=out/'malformed.gif';path.write_bytes(data[:rng.randrange(6,len(data))])
    p=subprocess.run([str(exe),str(path),str(out/'bad.ppm')],cwd=ROOT,env=env,capture_output=True,timeout=10)
    assert p.returncode in (0,1),p.stderr;malformed+=1
(out/'report.json').write_text(json.dumps({'images':results,'malformed_bounded_and_freed':malformed},indent=2));print(results,flush=True)
