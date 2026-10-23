"""Flash MegaCart mapper, running the actual boot ROM and low-RAM gateway.

OS CIO/SIO remains modeled as in machine.py. The bundled MyDOS initializer
and original R: relocator execute as 6502 code; this is not a full OS emulator.
"""
from machine import Machine, Memory, ROOT

class CartMemory(Memory):
    def __init__(self,rom):
        super().__init__();self.rom=bytes(rom);self.cart_bank=254
        self.switches=[];self.fetches=0;self.rom_writes=0
        self.cpu=None;self.burst_start=None;self.max_masked_cycles=0
    def __getitem__(self,a):
        if isinstance(a,slice):return super().__getitem__(a)
        a &= 65535
        if 0xd500<=a<=0xd51f:return self.cart_bank
        if self.cart_bank!=255 and 0x8000<=a<0xc000:
            assert not (self.bank&128 and 0xa000<=a<0xb000),'ROM and MEMAC A both drive the bus'
            self.fetches+=1
            return self.rom[self.cart_bank*16384+a-0x8000]
        return super().__getitem__(a)
    def __setitem__(self,a,v):
        if isinstance(a,slice):return super().__setitem__(a,v)
        a &=65535
        if 0xd500<=a<=0xd51f:
            if self.cpu and v not in (254,255):
                assert self.cpu.p&4 and self.ram[0xd40e]==0,'ROM mapped with IRQ/NMI enabled'
                if self.burst_start is None:self.burst_start=self.cpu.processorCycles
            if v==255 and self.cpu and self.burst_start is not None:
                self.max_masked_cycles=max(self.max_masked_cycles,self.cpu.processorCycles-self.burst_start)
                self.burst_start=None
            self.cart_bank=v&255;self.switches.append(v&255);return
        if self.cart_bank!=255 and 0x8000<=a<0xc000:
            self.rom_writes+=1
            raise AssertionError(f'Write to mapped cartridge at ${a:04x}')
        super().__setitem__(a,v)

class CartMachine(Machine):
    def __init__(self,base=0xd600,boot=True):
        build=ROOT/('build-cart' if base==0xd600 else 'build-cart-d700')
        rom=(build/f'PRISMTERM-{base:04X}.CAR').read_bytes()[16:]
        super().__init__(build_dir=build,memory=CartMemory(rom),initialize=False)
        self.mem.cpu=self.cpu
        # No XEX or font is preloaded: the cartridge must supply every byte.
        self.mem.ram[:]=bytes(65536)
        self.mem.ram[0xd301]=255
        self.mem.ram[0x2e7:0x2e9]=b'\x00\x07'
        self.mem.ram[0x22f]=0x22
        self.handles[0]=['E:',0,12]
        self.io_log=[];self.sio_log=[]
        if boot:self.boot()
    def cio(self):
        b=0x340+self.cpu.x;cmd=self.mem[b+2]
        p=self.mem[b+4]|self.mem[b+5]<<8
        name=self.string(p) if cmd==3 else self.handles.get(self.cpu.x,[''])[0]
        self.io_log.append((cmd,name))
        super().cio()
    def sio(self):
        self.sio_log.append((self.mem[0x300],self.mem[0x301],self.mem[0x302],self.mem[0x30a]|self.mem[0x30b]<<8))
        super().sio()
    def step(self):
        if self.cpu.pc==0xe453:  # OS DSKINV, with no physical disk attached.
            self.sio_log.append((0x31,self.mem[0x301],self.mem[0x302],0))
            self.ret(138);return
        super().step()
    def boot(self,limit=5000000):
        self.cpu.pc=self.mem[0xbffa]|self.mem[0xbffb]<<8
        for i in range(limit):
            if self.cpu.pc==self.addr('device_select'):
                self.boot_steps=i;return
            self.step()
        raise AssertionError(f'Cartridge boot stalled at ${self.cpu.pc:04x}, MEMLO={self.mem.ram[0x2e7:0x2e9].hex()}')
