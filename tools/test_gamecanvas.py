"""Actual ARM framebuffer/key oracle for an offscreen GameCanvas."""
import json
from arm_smoke import Machine, ROOT, finish_scan
def main():
    m=Machine(external_jar=ROOT/'build/tests/gamecanvas.jar',vxp_path=ROOT/'build/PocketJava-signed.vxp')
    m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    for _ in range(10):m.time+=33;m.call(m.callbacks['timer'],1)
    assert any(s=='BUFFER READY' for _,_,s,_ in m.rendered_texts)
    assert m.last.getpixel((30,110))!=(255,0,0),'Offscreen write appeared before flush'
    m.call(m.callbacks['key'],2,53);m.call(m.callbacks['key'],1,53)
    for _ in range(10):
        m.time+=33;m.call(m.callbacks['timer'],1)
        assert not m.cstr(m.symbol('status')),m.cstr(m.symbol('status'))
        if any(s=='GAMECANVAS PASS' for _,_,s,_ in m.rendered_texts):break
    else:raise AssertionError('GameCanvas callback did not finish')
    assert m.last.getpixel((30,110))==(255,0,0),'Flush did not copy offscreen pixels'
    m.save('gamecanvas-test.png')
    m.call(m.callbacks['key'],2,48)
    for _ in range(10):m.time+=33;m.call(m.callbacks['timer'],1)
    assert not m.cstr(m.symbol('status')),m.cstr(m.symbol('status'))
    assert m.last.getpixel((90,110))==(0,0,255),'Default paint did not show buffer'
    assert m.last.getpixel((30,110))==(255,255,255),'Default paint did not replace old canvas'
    m.call(m.callbacks['key'],2,53);m.call(m.callbacks['key'],1,53)
    for _ in range(10):m.time+=33;m.call(m.callbacks['timer'],1)
    assert not m.cstr(m.symbol('status')),m.cstr(m.symbol('status'))
    m.call(m.callbacks['system'],5,0);assert m.used==0
    report={'buffer_isolation':'passed','region_flush_pixels':'passed','rapid_press_release_latch':'passed','default_paint':'passed','suppressed_key_events':'passed','heap_after_quit':m.used,'heap_peak':m.peak}
    (ROOT/'build/gamecanvas-test.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':main()
