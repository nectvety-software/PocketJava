"""Execute the built ARM VXP with mocked MRE APIs (not a phone-speed benchmark).
Requires: pip install unicorn pyelftools Pillow
Exercises SDK symbol resolution, ELF relocation, event callbacks and bytecode.
"""
from pathlib import Path
from io import BytesIO
import struct,json,zipfile
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3,UC_ARM_REG_SP,UC_ARM_REG_LR,UC_ARM_REG_PC
from elftools.elf.elffile import ELFFile
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1]
BASE=0x1000000; HEAP=0x2000000; STACK=0x3000000; API=0x4000000; STOP=API+0xff00
class Machine:
    def __init__(self,external_jar=None,filesystem=None,vxp_path=None):
        self.u=Uc(UC_ARCH_ARM,UC_MODE_ARM)
        for addr,size in [(BASE,0x100000),(HEAP,0x200000),(STACK,0x10000),(API,0x10000)]:self.u.mem_map(addr,size)
        self.elf=ELFFile(BytesIO(Path(vxp_path or ROOT/'build/PocketJava.vxp').read_bytes()))
        # Release VXP is stripped; addresses come from its matching debug AXF.
        debug_elf=ELFFile(BytesIO((ROOT/'build/PocketJava.axf').read_bytes()))
        self.symbols={s.name:s['st_value'] for s in debug_elf.get_section_by_name('.symtab').iter_symbols()}
        for segment in self.elf.iter_segments():
            if segment['p_type']=='PT_LOAD':self.u.mem_write(BASE+segment['p_vaddr'],segment.data())
        relocations=self.elf.get_section_by_name('.rel.dyn')
        for r in relocations.iter_relocations():
            assert r['r_info_type']==23, f"Unhandled relocation {r['r_info_type']}"
            addr=BASE+r['r_offset'];self.put32(addr,self.get32(addr)+BASE)
        self.api={};self.handlers={};self.next_api=API;self.next_heap=HEAP;self.allocations={};self.freeblocks=[];self.used=0;self.peak=0
        self.callbacks={};self.time=0;self.texts=[];self.flushes=0;self.layer=None;self.last=None;self.files={};self.positions={};self.saved={};self.filepaths={};self.nextfile=1;self.seen=set()
        jarpath=Path(external_jar) if external_jar else ROOT/'demo/PocketDemo.jar'
        self.jar_name=jarpath.name;self.jar=jarpath.read_bytes();self.jad_name='PocketDemo.jad';self.jad=(ROOT/'demo/PocketDemo.jad').read_bytes();self.external_jar=external_jar
        if external_jar is None:
            # Default JAR stays above 64 KiB so opening it exercises the EOCD tail seek.
            buf=BytesIO()
            with zipfile.ZipFile(buf,'w') as out,zipfile.ZipFile(jarpath) as src:
                for info in src.infolist():out.writestr(info,src.read(info.filename))
                out.writestr('padding.bin',bytes(128*1024),compress_type=zipfile.ZIP_STORED)
            self.jar=buf.getvalue()
            # Keep the JAD's declared size in step with the padded JAR.
            self.jad=b'\n'.join(b'MIDlet-Jar-Size: '+str(len(self.jar)).encode() if line.startswith(b'MIDlet-Jar-Size:') else line for line in self.jad.split(b'\n'))
        self.filesystem=filesystem if filesystem is not None else {'E:\\'+self.jar_name:self.jar,**({'E:\\'+self.jad_name:self.jad} if not external_jar else {})};self.finders={};self.nextfinder=100;self.scan_patterns=[];self.drives=(ord('E'),ord('C'))
        self.font=ImageFont.truetype('C:/Windows/Fonts/consola.ttf',12)
        self.resolver=self.stub('resolver');self.u.hook_add(UC_HOOK_CODE,self.hook,begin=API,end=API+0xffff)
        self.u.mem_write(STOP,b'\x1e\xff\x2f\xe1')
    def put32(self,p,x):self.u.mem_write(p,struct.pack('<I',x&0xffffffff))
    def get32(self,p):return struct.unpack('<I',self.u.mem_read(p,4))[0]
    def cstr(self,p):
        b=bytearray()
        for i in range(512):
            x=self.u.mem_read(p+i,1)[0]
            if not x:break
            b.append(x)
        return b.decode('ascii')
    def wide(self,p):
        b=bytearray()
        for i in range(260):
            x=bytes(self.u.mem_read(p+2*i,2))
            if x==b'\0\0':break
            b.extend(x)
        return b.decode('utf-16le')
    def stub(self,name):
        if name not in self.api:
            addr=self.next_api;self.next_api+=4;self.api[name]=addr;self.handlers[addr]=name;self.u.mem_write(addr,b'\x1e\xff\x2f\xe1')
        return self.api[name]
    def malloc(self,n):
        n=(max(n,1)+7)&~7
        assert self.used+n<=1024*1024, 'MRE heap exhausted'
        p=None
        for i,(start,length) in enumerate(self.freeblocks):
            if length>=n:
                p=start;self.freeblocks.pop(i)
                if length>n:self.freeblocks.append((start+n,length-n))
                break
        if p is None:p=self.next_heap;self.next_heap+=n
        assert self.next_heap<HEAP+0x200000
        self.allocations[p]=n;self.used+=n;self.peak=max(self.peak,self.used);return p
    def free(self,p):
        if p:
            size=self.allocations.pop(p);self.used-=size;self.freeblocks.append((p,size));self.freeblocks.sort()
            merged=[]
            for start,length in self.freeblocks:
                if merged and merged[-1][0]+merged[-1][1]==start:merged[-1]=(merged[-1][0],merged[-1][1]+length)
                else:merged.append((start,length))
            self.freeblocks=merged
    def hook(self,u,addr,size,user):
        if addr==STOP:u.emu_stop();return
        name=self.handlers[addr];a=[u.reg_read(r) for r in (UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3)]
        a.extend(self.get32(u.reg_read(UC_ARM_REG_SP)+4*i) for i in range(4));result=0
        if name=='resolver':result=self.stub(self.cstr(a[0]))
        else:
            self.seen.add(name)
            if name=='vm_malloc':result=self.malloc(a[0])
            elif name=='vm_calloc':result=self.malloc(a[0]);u.mem_write(result,bytes(a[0]))
            elif name=='vm_realloc':
                old=self.allocations.get(a[0],0);data=bytes(u.mem_read(a[0],min(old,a[1]))) if a[0] else b'';self.free(a[0]);result=self.malloc(a[1]);u.mem_write(result,data)
            elif name=='vm_free':self.free(a[0])
            elif name=='vm_reg_sysevt_callback':self.callbacks['system']=a[0]
            elif name=='vm_reg_keyboard_callback':self.callbacks['key']=a[0]
            elif name=='vm_create_timer':self.callbacks['timer']=a[1];result=1
            elif name=='vm_delete_timer':self.callbacks.pop('timer',None)
            elif name=='vm_get_tick_count':result=self.time
            elif name=='vm_midi_set_type':pass
            elif name=='vm_midi_play_by_bytes':
                data=bytes(u.mem_read(a[0],a[1]));assert data[:4]==b'MThd' and data[14:18]==b'MTrk'
                assert int.from_bytes(data[18:22],'big')==len(data)-22
                self.tone_midi=data;result=1
            elif name=='vm_midi_stop':pass
            elif name=='vm_get_removeable_driver':result=self.drives[0]
            elif name=='vm_get_system_driver':result=self.drives[1]
            elif name=='vm_find_first':
                pattern=self.wide(a[0]);self.scan_patterns.append(pattern);directory=pattern[:-1].rstrip('\\')+'\\'
                entries=[p for p in self.filesystem if p.startswith(directory) and '\\' not in p[len(directory):]]
                if not entries:result=-1
                else:
                    result=self.nextfinder;self.nextfinder+=1;self.finders[result]=[entries,0]
                    u.mem_write(a[1],(entries[0]+'\0').encode('utf-16le'))
            elif name=='vm_find_next':
                entries,pos=self.finders[a[0]];pos+=1;self.finders[a[0]][1]=pos
                if pos==len(entries):result=-1
                else:u.mem_write(a[1],(entries[pos]+'\0').encode('utf-16le'))
            elif name=='vm_find_close':self.finders.pop(a[0])
            elif name=='vm_file_get_attributes':
                path=self.wide(a[0]);result=-1 if path not in self.filesystem else 16 if self.filesystem[path] is None else 0
            elif name=='vm_graphic_set_clip':pass
            elif name=='vm_graphic_create_layer':self.layer=self.malloc(240*320*2);result=1
            elif name=='vm_graphic_get_layer_buffer':result=self.layer
            elif name=='vm_graphic_delete_layer':self.free(self.layer);self.layer=None
            elif name=='vm_graphic_fill_rect':
                buf,x,y,w,h,_,color=a[:7]
                assert x+w<=240 and y+h<=320,(x,y,w,h)
                row=struct.pack('<H',color&0xffff)*w
                for yy in range(y,y+h):u.mem_write(buf+(yy*240+x)*2,row)
                if x==0 and y==0 and w==240 and h==320:self.texts=[]
            elif name=='vm_graphic_textout':self.texts.append((a[1],a[2],self.wide(a[3]),a[5]&0xffff))
            elif name=='vm_graphic_flush_layer':
                self.flushes+=1;data=u.mem_read(self.layer,240*320*2);rgb=bytearray()
                for c, in struct.iter_unpack('<H',data):rgb.extend(((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
                self.last=Image.frombytes('RGB',(240,320),bytes(rgb));draw=ImageDraw.Draw(self.last)
                for x,y,s,c in self.texts:draw.text((x,y),s,font=self.font,fill=((c>>11)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
                self.rendered_texts=self.texts;self.texts=[]
            elif name=='vm_file_open':
                path=self.wide(a[0]);data=self.filesystem.get(path)
                if path in self.saved:data=self.saved[path]
                if a[1]==4:data=b''
                if data is None:result=-1
                else:result=self.nextfile;self.nextfile+=1;self.files[result]=data;self.positions[result]=0;self.filepaths[result]=path
            elif name=='vm_file_write':
                pos=self.positions[a[0]];data=bytes(u.mem_read(a[1],a[2]));old=self.files[a[0]];self.files[a[0]]=old[:pos]+data+old[pos+len(data):];self.positions[a[0]]+=len(data);self.saved[self.filepaths[a[0]]]=self.files[a[0]];self.put32(a[3],len(data))
            elif name=='vm_file_getfilesize':self.put32(a[1],len(self.files[a[0]]));result=0
            elif name=='vm_file_seek':
                # Same contract as vmio.h: 0 on success, -1 for an unknown origin or out-of-range offset.
                offset=a[1] if a[1]<0x80000000 else a[1]-0x100000000;data=self.files[a[0]]
                target=offset if a[2]==1 else self.positions[a[0]]+offset if a[2]==2 else len(data)+offset if a[2]==3 else -1
                if a[2] not in (1,2,3) or target<0 or target>len(data):result=-1
                else:self.positions[a[0]]=target;result=0
            elif name=='vm_file_read':
                # Short reads are allowed; the runtime must loop until the buffer is full.
                pos=self.positions[a[0]];data=self.files[a[0]][pos:pos+min(a[2],997)];self.positions[a[0]]+=len(data);u.mem_write(a[1],data);self.put32(a[3],len(data));result=len(data)
            elif name=='vm_file_close':self.files.pop(a[0])
            elif name=='vm_exit_app':u.emu_stop()
            else:raise AssertionError(f'Unimplemented MRE API: {name}')
        u.reg_write(UC_ARM_REG_R0,result&0xffffffff)
    def call(self,address,*args):
        self.u.reg_write(UC_ARM_REG_SP,STACK+0xff00);self.u.reg_write(UC_ARM_REG_LR,STOP)
        for r,x in zip((UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3),args):self.u.reg_write(r,x)
        self.u.emu_start(address,0,count=300000000)
        assert self.u.reg_read(UC_ARM_REG_PC)==STOP, 'Callback exceeded instruction budget'
    def symbol(self,name):return BASE+self.symbols[name]
    def save(self,name):assert self.last is not None;self.last.save(ROOT/'build'/name)
def finish_scan(m):
    for _ in range(200):
        if not m.get32(m.symbol('scan_active')):return
        m.time+=33;m.call(m.callbacks['timer'],1)
    raise AssertionError('Scanner did not finish')

def main():
    m=Machine();m.call(m.symbol('gcc_entry'),m.resolver,0,0)
    m.call(m.callbacks['system'],1,0);finish_scan(m);m.save('arm-launcher.png')
    m.call(m.callbacks['key'],2,53)
    for i in range(12):m.time+=33;m.call(m.callbacks['timer'],1)
    assert any(s=='REAL MIDLET / BYTECODE' for _,_,s,_ in m.rendered_texts),m.texts
    m.save('arm-demo.png');before=m.last.copy()
    m.call(m.callbacks['key'],2,0xfffffffc)
    for i in range(4):m.time+=33;m.call(m.callbacks['timer'],1)
    assert before.getpixel((112,170))!=m.last.getpixel((112,170)),'Java key callback did not move sprite'
    m.call(m.callbacks['key'],2,42);m.save('arm-paused.png')
    assert any(s=='PAUSED' for _,_,s,_ in m.rendered_texts)
    m.call(m.callbacks['key'],2,48)
    # Select a genuine external JAR through the actual MRE file APIs.
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    for i in range(8):m.time+=33;m.call(m.callbacks['timer'],1)
    assert any(s=='REAL MIDLET / BYTECODE' for _,_,s,_ in m.rendered_texts),m.texts
    m.save('arm-external-jar.png')
    m.call(m.callbacks['key'],2,42);m.call(m.callbacks['key'],2,48)
    m.call(m.callbacks['key'],2,56);m.call(m.callbacks['key'],2,53)
    for i in range(8):m.time+=33;m.call(m.callbacks['timer'],1)
    assert any(s=='REAL MIDLET / BYTECODE' for _,_,s,_ in m.rendered_texts),m.texts
    m.save('arm-external-jad.png')
    m.call(m.callbacks['system'],6,0);assert m.layer is None and 'timer' not in m.callbacks
    m.call(m.callbacks['system'],1,0)
    for i in range(4):m.time+=33;m.call(m.callbacks['timer'],1)
    m.call(m.callbacks['system'],5,0);assert m.used==0,m.allocations
    report={'arm_vxp':'passed','builtin_jar':'passed','external_jar':'passed','external_jad':'passed','key_callback':'passed','pause_stop':'passed','hide_resume_quit':'passed','heap_peak_bytes_including_framebuffer':m.peak,'heap_after_quit':m.used,'screen_flushes':m.flushes,'api_names':sorted(m.seen),'limitation':'MRE APIs mocked; no device compatibility or hardware FPS claim'}
    (ROOT/'build/arm-smoke.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))
if __name__=='__main__':main()


