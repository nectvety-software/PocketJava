"""Execute AoE through ARM VXP; distinguish Java execution from verified gameplay."""
import argparse,hashlib,json
from pathlib import Path
from arm_smoke import Machine,ROOT,finish_scan
GAME=Path(r'D:\MRE\Porting-code\input\aoe\aoe-3-240x320.jar')
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--steps',type=int,default=1200);args=parser.parse_args()
    m=Machine(external_jar=GAME);m.call(m.symbol('gcc_entry'),m.resolver,0,0)
    m.call(m.callbacks['system'],1,0);finish_scan(m);m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    error='';i=-1
    for i in range(args.steps):
        if i in (350,650,950):m.call(m.callbacks['key'],2,53)
        m.time+=33;m.call(m.callbacks['timer'],1);error=m.cstr(m.symbol('status'))
        if i%100==0:print('callback',i,'heap',m.used,'error',error,flush=True)
        if error:break
    m.save('aoe-load-result.png');peak=m.peak;m.call(m.callbacks['system'],5,0)
    report={'game':str(GAME),'jar_sha256':hashlib.sha256(GAME.read_bytes()).hexdigest(),'vxp_sha256':hashlib.sha256((ROOT/'build/PocketJava.vxp').read_bytes()).hexdigest(),'callbacks':i+1,'error':error,'gameplay_verified':False,'heap_peak_including_framebuffer':peak,'heap_after_quit':m.used,'runtime':'ARM Unicorn with mocked MRE APIs; no hardware FPS claim'}
    (ROOT/'build/aoe-test.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2));assert m.used==0
if __name__=='__main__':main()
