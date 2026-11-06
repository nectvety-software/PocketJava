"""Ninja School 3 interaction capture, using genuine signed ARM bytecode."""
import hashlib,json
from pathlib import Path
from arm_smoke import Machine,ROOT,finish_scan

jar=Path('D:/Program/gamejava/j2me/NinjaSchool3_240x320.jar')
binary=ROOT/'build/PocketJava-signed.vxp'
out=ROOT/'build/ninja-scenario';out.mkdir(exist_ok=True)
m=Machine(external_jar=jar,vxp_path=binary)
report={'jar_sha256':hashlib.sha256(jar.read_bytes()).hexdigest(),'signed_vxp_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),'stages':[],'limitation':'ARM with mocked MRE; no hardware FPS or complete campaign claim'}
def stage(name,ticks,key=None):
    if key is not None:
        m.call(m.callbacks['key'],2,key&0xffffffff)
        for _ in range(3):m.time+=33;m.call(m.callbacks['timer'],1)
        m.call(m.callbacks['key'],1,key&0xffffffff)
    for _ in range(ticks):
        m.time+=33;m.call(m.callbacks['timer'],1)
        if m.cstr(m.symbol('status')):break
    m.save('ninja-scenario/'+name+'.png')
    result={'name':name,'key':key,'error':m.cstr(m.symbol('status')),'heap':m.used}
    report['stages'].append(result);print(result,flush=True)
    (out/'report.json').write_text(json.dumps(report,indent=2))
    assert not result['error'],result
try:
    m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    stage('01-audio',180)
    stage('02-quality',100,-7)
    stage('03-loaded',1800,-6)
    stage('04-confirm',100,-6)
    stage('05-fire',100,53)
    stage('06-right',100,54)
    stage('07-left',100,52)
finally:
    m.call(m.callbacks['system'],5,0)
    report['heap_peak_including_framebuffer']=m.peak;report['heap_after_quit']=m.used
    report['source_files_unchanged']=hashlib.sha256(jar.read_bytes()).hexdigest()==report['jar_sha256']
    (out/'report.json').write_text(json.dumps(report,indent=2))
