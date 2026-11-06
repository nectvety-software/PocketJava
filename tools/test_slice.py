"""Exercise the ARM time guard with a clock advancing at each SDK clock read."""
import json
from arm_smoke import Machine, ROOT, finish_scan
from profile_arm_game import arm_u64
class TimedMachine(Machine):
    timed=False
    def hook(self,u,address,size,user):
        if self.timed and self.handlers.get(address)=='vm_get_tick_count':self.time+=1
        super().hook(u,address,size,user)
def main():
    m=TimedMachine(external_jar=ROOT/'demo/PocketDemo.jar')
    m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    vm=m.get32(m.symbol('vm'));rows=[]
    for _ in range(12):
        before=arm_u64(m,'pj_instructions',vm);m.timed=True
        m.call(m.symbol('pj_step'),vm,12000,8);m.timed=False
        delta=arm_u64(m,'pj_instructions',vm)-before
        assert delta<=512,delta
        rows.append(delta)
        m.call(m.symbol('pj_repaint'),vm)
    assert sum(rows)>0
    m.call(m.callbacks['system'],5,0);assert m.used==0
    report={'opcode_cap':12000,'time_limit_ms':8,'clock_step_ms':1,'instructions_per_slice':rows,'heap_after_quit':m.used,'limitation':'Synthetic clock verifies scheduling guard, not native decoder/GC duration or hardware FPS'}
    (ROOT/'build/gameplay-slice-test.json').write_text(json.dumps(report,indent=2));print(report)
if __name__=='__main__':main()
