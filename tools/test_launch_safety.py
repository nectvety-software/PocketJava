"""Guard 8 KiB ARM stack and inject MRE allocation failures during Java launch."""
import hashlib,json
from arm_smoke import Machine, ROOT, STACK, finish_scan
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_LR, UC_ARM_REG_PC

class FaultMachine(Machine):
    active=False
    fail_at=0
    allocations_seen=0
    def hook(self,u,address,size,user):
        name=self.handlers.get(address)
        if self.active and name in ('vm_malloc','vm_calloc','vm_realloc'):
            self.allocations_seen+=1
            if self.allocations_seen==self.fail_at:
                u.reg_write(UC_ARM_REG_R0,0);u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR));return
        super().hook(u,address,size,user)

def start(m):
    m.u.mem_unmap(STACK,0xe000) # Guard below the final 8 KiB mapped stack page pair.
    m.call(m.symbol('gcc_entry'),m.resolver,0,0);m.call(m.callbacks['system'],1,0);finish_scan(m)

def main():
    binary=ROOT/'build/PocketJava-signed.vxp';results=[]
    for jar,label in ((ROOT/'demo/PocketDemo.jar','REAL MIDLET / BYTECODE'),(ROOT/'build/tests/library.jar','LIBRARY PASS')):
        m=Machine(external_jar=jar,vxp_path=binary);start(m)
        m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
        for _ in range(100):
            m.time+=33;m.call(m.callbacks['timer'],1)
            assert not m.cstr(m.symbol('status')),m.cstr(m.symbol('status'))
            if any(text==label for _,_,text,_ in m.rendered_texts):break
        else:raise AssertionError('8 KiB stack fixture did not complete: '+label)
        m.call(m.callbacks['system'],5,0);assert m.used==0
        results.append(dict(jar=jar.name,stack_bytes=8192,passed=True,heap_after_quit=m.used))
    faults=[]
    rejected=[]
    for jar in (ROOT/'build/tests/deep.jar',ROOT/'build/tests/truncated.jar'):
        m=Machine(external_jar=jar,vxp_path=binary);start(m)
        m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
        error=m.cstr(m.symbol('status'));assert error,(jar.name,'Expected launch rejection')
        assert m.get32(m.symbol('mode'))==0
        m.call(m.callbacks['system'],5,0);assert m.used==0
        rejected.append(dict(jar=jar.name,error=error,heap_after_quit=m.used))
    for fail_at in range(1,17):
        m=FaultMachine(vxp_path=binary);start(m);m.fail_at=fail_at;m.active=True
        m.call(m.callbacks['key'],2,53)
        for _ in range(5):m.time+=33;m.call(m.callbacks['timer'],1)
        error=m.cstr(m.symbol('status'));m.active=False
        m.call(m.callbacks['system'],5,0);assert m.used==0,(fail_at,m.used)
        faults.append(dict(allocation=fail_at,error=error,heap_after_quit=m.used))
    report=dict(signed_vxp_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),native_stack_tests=results,allocation_faults=faults,rejected_archives=rejected,
                limitation='ARM mocked MRE; Nokia 225 Dual SIM hardware confirmation still required')
    (ROOT/'build/launch-safety.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report))

if __name__=='__main__':main()
