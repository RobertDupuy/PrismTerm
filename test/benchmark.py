#!/usr/bin/env python3
"""Deterministic CPU-cycle benchmarks; no emulated SIO or blitter wait time."""
import hashlib,json,pathlib,sys
from machine import Machine,ROOT
from build_baseline import build
COMMIT='c7d8bbc2c95718bb9fc0847fb8fe43ba42b9f60f'
def machine(variant):
    return Machine(ROOT/'baseline/upstream',ROOT/'baseline/compiled') if variant=='upstream' else Machine()
def receive(m,data):
    m.set('device_type',1);m.set('n_unit',1);m.set('lf_mode',0);m.incoming+=data;n=0
    while m.incoming:n+=m.call('recv_from_device')
    m.call('cursor_off');return n

def main():
    build();results=[]
    for name,data in [('Native text, 1200 bytes',b'A'*1200),('100 lines, 7800 printable bytes',((b'0123456789'*7)+b'ABCDEFGH\r\n')*100),('ANSI color text, 100 lines',b''.join((f'\x1b[{30+(r%8)}m'.encode()+b'BBS color text: '+b'.'*55+b'\r\n') for r in range(100)))]:
        values={};screens=[]
        for variant in ('upstream','plus'):
            m=machine(variant);values[variant]=receive(m,data);screens.append(m.screen())
        assert screens[0]==screens[1],name
        results.append(dict(name=name,unit='6502 cycles',bytes=len(data),**values))
    for name,fn in [('Cursor address at row 23',lambda m:(m.set('row',23),m.set('column',79),m.call('recalc_cursor'))[-1]),('Scroll rows 3-21 upward by 3',lambda m:m.call('scroll_rgn_up',a=3,x=21,y=3))]:
        results.append(dict(name=name,unit='6502 cycles',**{v:fn(machine(v)) for v in ('upstream','plus')}))
    values={}
    for v in ('upstream','plus'):
        m=machine(v);n=m.mem.blit_bytes;m.call('scroll_1d');values[v]=m.mem.blit_bytes-n
    results.append(dict(name='Full-screen one-line scroll',unit='blitter bytes written',**values))
    for r in results:r['reduction_percent']=round(100*(1-r['plus']/r['upstream']),2)
    m=Machine();m.call('plus_enable_bitmap');m.call('plus_dirty_all');graphics=m.call('plus_render_dirty')
    info={'baseline_commit':COMMIT,'cpu':'NMOS 6502, py65 instruction cycle accounting','limits':'CIO/SIO and VBXE blits are functional models. Their real waiting time and Atari DMA/interrupt contention are not included. These numbers are not measured baud rates or hardware throughput.','benchmarks':results,'graphics_full_80_column_frame_cpu_cycles':graphics,'binary_sha256':hashlib.sha256((ROOT/'build/PRISM.XEX').read_bytes()).hexdigest()}
    (ROOT/'build/benchmark.json').write_text(json.dumps(info,indent=2)+'\n')
    lines=['# Performance measurements','',f'Pinned upstream: `{COMMIT}`. PrismTerm is compiled from the current source; the comparison source is recovered and hash-verified from the original Git history.','',info['limits'],'','| Workload | Unit | Upstream | PrismTerm | Reduction |','|---|---|---:|---:|---:|']
    for r in results:lines.append(f"| {r['name']} | {r['unit']} | {r['upstream']:,} | {r['plus']:,} | {r['reduction_percent']}% |")
    lines+=['',f'An initial complete 80-column graphics frame costs {graphics:,} modeled CPU cycles, plus hardware blitter time. Subsequent writes redraw only the dirty cell spans. Native text remains the normal 80-column path.','','Run `make benchmark` to reproduce. Test inputs are in `test/benchmark.py`.']
    (ROOT/'PERFORMANCE.md').write_text('\n'.join(lines)+'\n')
    print('\n'.join(lines))
if __name__=='__main__':main()
