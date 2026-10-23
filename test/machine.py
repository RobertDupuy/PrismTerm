"""Runs the shipped XEX instructions. Models MEMAC, copy blits and OS I/O.
CPU cycle totals are exact py65 counts; no claim of cycle-accurate VBXE/SIO timing.
"""
import pathlib,re,struct
from py65.devices.mpu6502 import MPU
ROOT=pathlib.Path(__file__).resolve().parents[1]
class Memory:
    def __init__(self):
        self.ram=bytearray(65536); self.vram=bytearray(524288)
        self.base=0xd600;self.bank=255;self.bank_b=0; self.reg=bytearray(256); self.blits=0; self.blit_bytes=0
        self.palette=[bytearray(768) for _ in range(4)];self.pi=0;self.pc=0
    def __getitem__(self,a):
        if isinstance(a,slice): return [self[i] for i in range(*a.indices(65536))]
        a &= 65535
        if 0xa000<=a<0xb000 and self.bank&128:return self.vram[((self.bank&127)<<12)+(a&4095)]
        if 0x4000<=a<0x8000 and self.bank_b&128:return self.vram[((self.bank_b&31)<<14)+(a&16383)]
        if a==self.base+0x5f:return self.bank
        if a==self.base+0x40:return 0x21
        if a==self.base+0x53:return 0
        if a==0xd01f:return 7
        return self.ram[a]
    def __setitem__(self,a,v):
        if isinstance(a,slice):
            for i,b in zip(range(*a.indices(65536)),v):self[i]=b
            return
        a &=65535;v &=255
        if 0xa000<=a<0xb000 and self.bank&128:self.vram[((self.bank&127)<<12)+(a&4095)]=v;return
        if 0x4000<=a<0x8000 and self.bank_b&128:self.vram[((self.bank_b&31)<<14)+(a&16383)]=v;return
        self.ram[a]=v
        if a==self.base+0x5d:self.bank_b=v
        if a==self.base+0x5f:self.bank=v
        if self.base<=a<self.base+256:self.reg[a&255]=v
        if a==self.base+0x44:self.pc=v
        if a==self.base+0x45:self.pi=v&3
        if self.base+0x46<=a<=self.base+0x48:
            self.palette[self.pi][self.pc*3+a-self.base-0x46]=v
            if a==self.base+0x48:self.pc=(self.pc+1)&255
        if a==self.base+0x53 and v&1:self.blit()
    def blit(self):
        p=int.from_bytes(self.reg[0x50:0x53],'little')&0x7ffff
        for chain in range(64):
            b=self.vram[p:p+21]
            src=int.from_bytes(b[0:3],'little'); sy=int.from_bytes(b[3:5],'little',signed=True);sx=b[5] if b[5]<128 else b[5]-256
            dst=int.from_bytes(b[6:9],'little');dy=int.from_bytes(b[9:11],'little',signed=True);dx=b[11] if b[11]<128 else b[11]-256
            w=int.from_bytes(b[12:14],'little')+1;h=b[14]+1
            assert w<=512 and h<=256,(hex(p),list(b))
            mode=b[20]&7
            for y in range(h):
                for x in range(w):
                    s=(self.vram[(src+y*sy+x*sx)&0x7ffff]&b[15])^b[16]
                    d=(dst+y*dy+x*dx)&0x7ffff
                    if mode==0:self.vram[d]=s
                    elif mode==1:
                        if s:self.vram[d]=s
                    elif mode==3:self.vram[d]|=s
                    elif mode==4:self.vram[d]&=s
                    elif mode==5:self.vram[d]^=s
                    else:raise AssertionError('Unmodeled blitter mode '+str(mode))
            self.blits+=1;self.blit_bytes+=w*h
            if not b[20]&8:return
            p+=21
        raise AssertionError('Blitter chain did not terminate')
class Machine:
    def __init__(self,root=ROOT,build_dir=None,memory=None,initialize=True):
        self.root=pathlib.Path(root);self.mem=memory if memory is not None else Memory();self.cpu=MPU(memory=self.mem)
        build_dir=pathlib.Path(build_dir) if build_dir else self.root/'build'
        self.syms={}
        for line in (build_dir/'symbols.lbl').read_text().splitlines():
            m=re.match(r'al ([0-9A-Fa-f]+) \.(.*)',line)
            if m:self.syms[m[2]]=int(m[1],16)
        for line in (build_dir/'program.dbg').read_text().splitlines():
            if not line.startswith('sym\t'):continue
            n=re.search(r'name="([^"]+)"',line);v=re.search(r'val=(0x[\da-fA-F]+|\d+)',line)
            if n and v and n[1] not in self.syms:self.syms[n[1]]=int(v[1],0)
        self.mem.base=self.syms['core_version']&0xff00
        b=(build_dir/'PRISM.XEX').read_bytes();i=0
        while i<len(b):
            a=int.from_bytes(b[i:i+2],'little');i+=2
            if a==65535:continue
            z=int.from_bytes(b[i:i+2],'little');i+=2
            self.mem.ram[a:z+1]=b[i:i+z-a+1];i+=z-a+1
        self.build_dir=build_dir
        self.out=bytearray();self.incoming=bytearray();self.files={};self.handles={};self.keys=[];self.dialog_keys=[]
        if not initialize:return
        self.mem.ram[0xd301]=255
        self.mem.vram[:2048]=(self.root/'disk/IBMPC.FNT').read_bytes()
        x=self.syms['xdl'];n=self.syms['bcb_end']-x
        self.mem.vram[0x800:0x800+n]=self.mem.ram[x:x+n]
        self.cpu.sp=0xff
        self.set('text_color',0x87);self.set('term_last_col',79);self.set('scroll_bot',23);self.set('active_font',3)
        self.call('recalc_cursor');self.call('scroll_page')
        if 'plus_init' in self.syms:self.call('plus_init')
    def addr(self,k):return self.syms[k]
    def set(self,k,v):self.mem[self.addr(k)]=v
    def get(self,k):return self.mem[self.addr(k)]
    def call(self,k,a=None,x=None,y=None,limit=4000000):
        c=self.cpu
        if a is not None:c.a=a
        if x is not None:c.x=x
        if y is not None:c.y=y
        oldsp=c.sp;c.stPushWord(0x05ff);c.pc=self.addr(k)
        cycles=c.processorCycles
        for count in range(limit):
            if c.pc==0x600:
                assert c.sp==oldsp,(k,'stack leak',oldsp,c.sp)
                return c.processorCycles-cycles
            self.step()
        raise AssertionError(f'{k} instruction limit at ${c.pc:04x}')
    def step(self):
        c=self.cpu
        if self.dialog_keys and not self.mem.bank_b and c.pc==self.syms.get('plus_key'):
            value=self.dialog_keys.pop(0);table=self.addr('keycode_table')
            raw=next(k for k in range(256) if self.mem[table+k]==value)
            self.set('menu_key',raw);self.set('menu_key_ready',1)
        self.mem.ram[0x14]=(c.processorCycles//29868)&255
        if c.pc==0xe456:self.cio();return
        if c.pc==0xe459:self.sio();return
        c.step()
    def ret(self,status=1):
        self.cpu.y=status;self.cpu.p=(self.cpu.p&0x7d)|(status&0x80)|(2 if not status else 0)
        self.cpu.pc=(self.cpu.stPopWord()+1)&65535
    def string(self,p,end=0x9b):
        out=bytearray()
        for i in range(256):
            b=self.mem[p+i]
            if b in (0,end):break
            out.append(b)
        return out.decode('ascii','replace')
    def cio(self):
        x=self.cpu.x;b=0x340+x;cmd=self.mem[b+2];p=self.mem[b+4]|self.mem[b+5]<<8;n=self.mem[b+8]|self.mem[b+9]<<8
        if cmd==3:
            name=self.string(p);mode=self.mem[b+10]
            if name.startswith('K:'):self.handles[x]=[name,0,mode]
            elif re.match(r'^R[1-8]?:',name):self.handles[x]=[name,0,mode]
            elif mode==4 and name not in self.files:
                path=(self.build_dir/name[2:]) if name in ('D:TRANS.BIN','D:UI.BIN') else self.root/'disk'/name.split(':')[-1]
                if not path.exists():self.ret(170);return
                self.files[name]=bytearray(path.read_bytes())
            if mode in (8,9):
                if mode==8 or name not in self.files:self.files[name]=bytearray()
            self.handles[x]=[name,len(self.files.get(name,b'')) if mode==9 else 0,mode]
        elif cmd==12:self.handles.pop(x,None)
        elif cmd in (7,5):
            name,pos,mode=self.handles.get(x,['R:',0,4])
            if re.match(r'^R[1-8]?:',name):data=self.incoming[:n];del self.incoming[:n]
            elif name.startswith('K:'):data=bytes([self.keys.pop(0)]) if self.keys else b''
            else:data=self.files[name][pos:pos+n];self.handles[x][1]+=len(data)
            if not n:
                self.cpu.a=data[0] if data else 0
            else:self.mem[p:p+len(data)]=data
            self.mem[b+8]=len(data)&255;self.mem[b+9]=len(data)>>8
            if len(data)<n:self.ret(136);return
        elif cmd in (11,9):
            data=bytes(self.mem[p:p+n]) if n else bytes([self.cpu.a])
            name,pos,mode=self.handles.get(x,['R:',0,8])
            if re.match(r'^R[1-8]?:',name):self.out+=data
            else:self.files.setdefault(name,bytearray()).extend(data)
        elif cmd==13:
            self.mem[0x2ea]=0;self.mem[0x2eb]=len(self.incoming)&255;self.mem[0x2ec]=len(self.incoming)>>8
        self.ret()
    def sio(self):
        cmd=self.mem[0x302];p=self.mem[0x304]|self.mem[0x305]<<8;n=self.mem[0x308]|self.mem[0x309]<<8
        if self.mem[0x300]==0x31:
            path=self.root/'fonts/UNICODE.ATR'
            if path.exists():
                if not hasattr(self,'font_atr'):self.font_atr=path.read_bytes()
                sector=self.mem[0x30a]|self.mem[0x30b]<<8
                off=16+(sector-1)*128 if sector<4 else 16+384+(sector-4)*256
                data=self.font_atr[off:off+n];self.mem[p:p+len(data)]=data
                self.mem[0x303]=1 if len(data)==n else 144
                self.ret(self.mem[0x303]);return
            self.mem[0x303]=144;self.ret(144);return
        if cmd==ord('W'):self.out+=bytes(self.mem[p:p+n])
        elif cmd==ord('R'):
            data=self.incoming[:n];del self.incoming[:n];self.mem[p:p+len(data)]=data
        elif cmd==ord('S'):self.mem[p:p+4]=[len(self.incoming)&255,len(self.incoming)>>8,1,1]
        self.mem[0x303]=1;self.ret()
    def feed(self,data):
        if isinstance(data,str):data=data.encode()
        cycles=0
        for b in data:
            self.set('temp_char',b);cycles+=self.call('process_char')
        return cycles
    def screen(self):return [bytes(self.mem.vram[0x7f100+r*160:0x7f100+(r+1)*160:2]) for r in range(24)]
