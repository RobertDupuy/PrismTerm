import pathlib,subprocess,re,os
for attempt in range(12):
 r=subprocess.run(['ca65','-g','ANSIVBXE_ca65.asm','-o','build/main.o'],capture_output=True,text=True)
 if not r.returncode:break
 errors=re.findall(r'([^\n(]+)\((\d+)\): Error: Range error',r.stderr)
 if not errors:raise SystemExit(r.stderr)
 for fn,ln in errors:
  f=pathlib.Path(fn);lines=f.read_text().splitlines();i=int(ln)-1;lines[i]=re.sub(r'\bb(eq|ne|cc|cs|pl|mi|vc|vs)\b',r'j\1',lines[i]);f.write_text('\n'.join(lines)+'\n')
else:raise SystemExit('branch conversion did not converge')
