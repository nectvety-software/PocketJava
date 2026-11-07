"""Reproducible native host / ARM VXP build. No shell interpolation."""
import argparse,json,os,pathlib,shutil,subprocess,hashlib,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def run(args,**kwargs):subprocess.run([str(x) for x in args],check=True,cwd=ROOT,**kwargs)
def compiler(name, fallback=None):
    path=shutil.which(name)
    if not path and fallback and pathlib.Path(fallback).exists():path=fallback
    if not path:raise SystemExit(f'Missing compiler: {name}')
    return path
def main():
    p=argparse.ArgumentParser();p.add_argument('target',choices=['host','vxp','all']);p.add_argument('--imsi',default='');args=p.parse_args()
    if args.imsi and (not args.imsi.isdigit() or len(args.imsi) not in (15,16)):p.error('IMSI must be 15 digits, or Nokia-prefixed 16 digits')
    run([sys.executable,ROOT/'tools/build_demo.py'])
    out=ROOT/'build';out.mkdir(exist_ok=True)
    sources=['native/memory.c','native/jar.c','native/vm.c','vendor/miniz/miniz_tinfl.c']
    defines=['-DMINIZ_NO_STDIO','-DMINIZ_NO_TIME','-DMINIZ_NO_MALLOC','-DMINIZ_USE_UNALIGNED_LOADS_AND_STORES=0','-DNDEBUG']
    common=['-std=c99','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-Inative','-Ivendor/miniz',*defines]
    if args.target in ('host','all'):
        gcc=compiler('gcc',r'C:\msys64\mingw64\bin\gcc.exe')
        os.environ['PATH']=str(pathlib.Path(gcc).parent)+os.pathsep+os.environ.get('PATH','')
        run([gcc,*common,*sources,'native/host.c','-o',out/'pocketjava.exe'])
    if args.target in ('vxp','all'):
        gcc=compiler('arm-none-eabi-gcc');objcopy=compiler('arm-none-eabi-objcopy');sdk=ROOT/'vendor/mre/sdk';objs=[]
        flags=['-fstack-usage','-fpic','-march=armv5te','-marm','-mlittle-endian','-fvisibility=hidden','-ffunction-sections','-fdata-sections','-DMRE','-DGCC','-D__MRE_COMPILER_GCC__','-I'+str(sdk/'include')]
        for source in sources+['native/mre.c','native/entry.c']:
            obj=out/(pathlib.Path(source).stem+'.o');run([gcc,*common,*flags,'-c',source,'-o',obj]);objs.append(obj)
        axf=out/'PocketJava.axf'
        run([gcc,'-march=armv5te','-marm','-fpic','-nostartfiles','-pie','-Wl,--gc-sections','-Wl,--no-warn-rwx-segments','-Wl,-Map='+str(out/'PocketJava.map'),'-T',sdk/'scat.ld','-o',axf,*objs,'-Wl,--start-group',*sorted((sdk/'lib').glob('*.a')),'-lc','-lgcc','-Wl,--end-group'])
        res=out/'PocketJava-res.vxp';run([objcopy,'--strip-unneeded','--add-section','.vm_res='+str(sdk/'resource.bin'),axf,res])
        packdir=out/'pack';packdir.mkdir(exist_ok=True)
        manifest=dict(name='PocketJava Preview',vendor='PocketJava',app_name_multilang=['PocketJava'],description='Experimental Java MIDlet interpreter',version=1,required_ram=2048,imsi=args.imsi,run_in_background=0)
        (packdir/'manifest.json').write_text(json.dumps(manifest),encoding='utf8')
        target=out/('PocketJava-signed.vxp' if args.imsi else 'PocketJava.vxp')
        subprocess.run([sys.executable,str(sdk/'build.py'),str(res),str(target)],check=True,cwd=packdir)
        digest=hashlib.sha256(target.read_bytes()).hexdigest();target.with_suffix('.vxp.sha256').write_text(digest+'  '+target.name+'\n')
        print(f'Built {target} ({target.stat().st_size} bytes); IMSI binding: {bool(args.imsi)}')
if __name__=='__main__':main()
