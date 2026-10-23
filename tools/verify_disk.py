#!/usr/bin/env python3
"""Verify the generated 720-sector DD ATR against its staging directory."""
import pathlib,struct,sys

def verify(image,directory):
    data=pathlib.Path(image).read_bytes();directory=pathlib.Path(directory)
    assert data[:2]==b'\x96\x02' and int.from_bytes(data[4:6],'little')==256
    assert len(data)==16+3*128+717*256
    def sector(n):
        assert 1<=n<=720
        start=16+(n-1)*128 if n<=3 else 16+384+(n-4)*256
        return data[start:start+(128 if n<=3 else 256)]
    entries={}
    for n in range(361,369):
        directory_block=sector(n)
        for off in range(0,128,16):
            row=directory_block[off:off+16]
            if not row[0]&0x40 or row[0]&0x80:continue
            name=row[5:13].decode('ascii').rstrip();ext=row[13:16].decode('ascii').rstrip();name+='.'+ext if ext else ''
            nsectors=int.from_bytes(row[1:3],'little');current=int.from_bytes(row[3:5],'little');seen=set();out=bytearray()
            while current:
                assert current not in seen;seen.add(current);block=sector(current)
                used=block[-1];assert used<=len(block)-3;out+=block[:used]
                current=((block[-3]&3)<<8)|block[-2]
            assert len(seen)==nsectors,name
            assert bytes(out)==(directory/name).read_bytes(),name
            entries[name]=len(out)
    assert set(entries)=={p.name for p in directory.iterdir() if p.is_file()}
    assert {'DOS.SYS','DUP.SYS','RS232.AR0','PRISM.AR1'}<=set(entries)
    print(f'{pathlib.Path(image).name}: all {len(entries)} files and sector chains match staging bytes')
if __name__=='__main__':verify(*sys.argv[1:])
