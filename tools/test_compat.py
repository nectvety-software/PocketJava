"""ARM monitor semantics and local relative/encoded JAD path resolution."""
import json
from arm_smoke import Machine, ROOT, finish_scan
def open_machine(m):
    m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
def run_until(m,label):
    for _ in range(100):
        m.time+=33;m.call(m.callbacks['timer'],1)
        assert not m.cstr(m.symbol('status')),m.cstr(m.symbol('status'))
        if any(s==label for _,_,s,_ in m.rendered_texts):break
    else:raise AssertionError('Missing '+label)
    m.call(m.callbacks['system'],5,0);assert m.used==0
def main():
    m=Machine(external_jar=ROOT/'build/tests/monitors.jar',vxp_path=ROOT/'build/PocketJava-signed.vxp');open_machine(m);run_until(m,'MONITORS PASS')
    jar=(ROOT/'demo/PocketDemo.jar').read_bytes()
    fs={'E:\\entry.JaD':b'MIDlet-Jar-URL: games/demo%20game.jar\n','E:\\games':None,'E:\\games\\demo game.jar':jar}
    m=Machine(filesystem=fs,vxp_path=ROOT/'build/PocketJava-signed.vxp');open_machine(m);run_until(m,'REAL MIDLET / BYTECODE')
    assert 'E:\\games\\demo game.jar' in m.filepaths.values(),m.filepaths
    report={'arm_monitors':'passed','arm_relative_encoded_jad':'passed','heap_after_quit':m.used}
    (ROOT/'build/compat-arm-test.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':main()
