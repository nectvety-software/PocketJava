"""Signed ARM game run with synthetic time advancing on SDK clock reads."""
import argparse
import hashlib
import json
from pathlib import Path
from arm_smoke import ROOT, finish_scan
from test_slice import TimedMachine

def main():
    parser=argparse.ArgumentParser();parser.add_argument('jar',type=Path)
    parser.add_argument('--callbacks',type=int,default=400)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    binary=ROOT/'build/PocketJava-signed.vxp';source_hash=hashlib.sha256(args.jar.read_bytes()).hexdigest()
    m=TimedMachine(external_jar=args.jar,vxp_path=binary)
    report=dict(path=str(args.jar),sha256=source_hash,signed_vxp_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                clock='1 synthetic ms per SDK clock read, plus 33 ms between callbacks',steps_limit=args.callbacks,
                gameplay_verified=False,limitation='Synthetic scheduling check; not device FPS or a fixed-clock budget comparison')
    try:
        m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)
        m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53);m.timed=True
        callbacks=0
        for step in range(args.callbacks):
            if step in (350,650,950):m.call(m.callbacks['key'],2,53);m.call(m.callbacks['key'],1,53)
            m.time+=33;m.call(m.callbacks['timer'],1);callbacks+=1
            if m.cstr(m.symbol('status')):break
        report.update(callbacks=callbacks,error=m.cstr(m.symbol('status')),heap_peak_including_framebuffer=m.peak,
                      last_text=[text for _,_,text,_ in m.rendered_texts])
        report['no_error_within_budget']=not report['error']
    except Exception as error:report['harness_error']=str(error)
    finally:
        m.call(m.callbacks['system'],5,0);report['heap_after_quit']=m.used
    assert hashlib.sha256(args.jar.read_bytes()).hexdigest()==source_hash
    report['source_files_unchanged']=True
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(report,ensure_ascii=True))

if __name__=='__main__':main()
