"""Keyboard driver for the compiled local UI, also used by recorded demos."""
class MenuDriver:
    def __init__(self,m,category=0):
        self.m=m;self.sp=m.cpu.sp
        m.cpu.stPushWord(0x05ff);m.cpu.a=category;m.cpu.pc=m.addr('ui_open')
        self.advance()
    def advance(self,limit=12000000):
        m=self.m
        for _ in range(limit):
            if m.cpu.pc==0x600:
                assert m.cpu.sp==self.sp,('menu stack leak',self.sp,m.cpu.sp)
                return False
            if not m.mem.bank_b and m.cpu.pc==m.addr('plus_key') and not m.dialog_keys:return True
            m.step()
        raise AssertionError(f'menu stalled at ${m.cpu.pc:04x}')
    def press(self,keys):
        if isinstance(keys,str):keys=keys.encode('ascii')
        self.m.dialog_keys.extend(keys)
        return self.advance()
    def text(self):return '\n'.join(r.decode('cp437') for r in self.m.screen())
