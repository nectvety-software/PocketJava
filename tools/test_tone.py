"""Verify the actual ARM tone bridge and its MIDI bytes; no audible-output claim."""
import json
from arm_smoke import Machine, ROOT, finish_scan

def main():
    m=Machine(external_jar=ROOT/'build/tests/library.jar')
    m.call(m.symbol('gcc_entry'),m.resolver,0,0)
    m.call(m.callbacks['system'],1,0);finish_scan(m)
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    for _ in range(300):
        m.time+=33;m.call(m.callbacks['timer'],1)
        assert not m.cstr(m.symbol('status')),m.cstr(m.symbol('status'))
        if any(s=='LIBRARY PASS' for _,_,s,_ in m.rendered_texts):break
    else:raise AssertionError('ARM library tests did not finish')
    data=m.tone_midi
    assert data[22:31]==bytes([0,255,81,3,7,161,32,0,144])
    assert data[31:]==bytes([69,101,129,122,128,69,0,0,255,47,0]),data.hex()
    m.call(m.callbacks['key'],2,42)
    assert 'vm_midi_stop' in m.seen
    m.call(m.callbacks['system'],5,0);assert m.used==0
    report={'arm_library':'passed','midi_note':69,'duration_ms':250,'velocity':101,'midi_bytes':len(data),'pause_stop':'passed','heap_after_quit':m.used,'limitation':'Mocked MIDI API; audible sound requires a real MRE device'}
    (ROOT/'build/tone-test.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

if __name__=='__main__':main()
