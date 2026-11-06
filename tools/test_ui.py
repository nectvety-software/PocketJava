"""ARM launcher navigation and captures for the RetroMRE-style UI."""
import json
from arm_smoke import Machine, ROOT, finish_scan
def main():
    m=Machine(vxp_path=ROOT/'build/PocketJava-signed.vxp')
    m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)
    folder=ROOT/'build/ui-preview';folder.mkdir(exist_ok=True)
    def key(code):m.call(m.callbacks['key'],2,code)
    def save(name):m.save('ui-preview/'+name+'.png')
    save('games');key(54);assert m.get32(m.symbol('ui_tab'))==1;save('jar')
    key(54);assert m.get32(m.symbol('ui_tab'))==2 and m.get32(m.symbol('selected'))==2;save('jad')
    key(53)
    for _ in range(8):m.time+=33;m.call(m.callbacks['timer'],1)
    assert any(s=='REAL MIDLET / BYTECODE' for _,_,s,_ in m.rendered_texts)
    key(42);save('pause');key(48)
    key(54);assert m.get32(m.symbol('ui_tab'))==3;save('info')
    key(54);assert m.get32(m.symbol('ui_tab'))==0
    key(0xfffffffa);assert m.get32(m.symbol('ui_options'))==1;save('options')
    key(56);key(53);finish_scan(m);assert m.get32(m.symbol('count'))==2
    assert m.get32(m.symbol('ui_options'))==0
    m.call(m.callbacks['system'],5,0);assert m.used==0
    report={'style':'RetroMRE paper/orange','tabs_and_jad_open':'passed','options_rescan':'passed','pause':'passed','heap_after_quit':m.used}
    (folder/'report.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':main()
