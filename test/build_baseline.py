#!/usr/bin/env python3
"""Compile the pinned, unchanged upstream ca65 source for comparisons."""
import hashlib,json,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[1]
COMMIT='c7d8bbc2c95718bb9fc0847fb8fe43ba42b9f60f'

def materialize():
    """Recover the comparison source from the fork's original Git history."""
    src=ROOT/'baseline/upstream'
    manifest=json.loads((ROOT/'baseline/upstream-tree.json').read_text())
    assert manifest['sha']==COMMIT, 'Unexpected upstream revision'
    for item in manifest['tree']:
        if item['type']!='blob':continue
        path=pathlib.PurePosixPath(item['path'])
        if path.is_absolute() or '..' in path.parts:
            raise ValueError('Unsafe path in baseline manifest')
        dest=src/path
        def digest(data):
            return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
        if dest.is_file() and digest(dest.read_bytes())==item['sha']:continue
        try:
            data=subprocess.check_output(['git','show',f'{COMMIT}:{path}'],cwd=ROOT,stderr=subprocess.PIPE)
        except subprocess.CalledProcessError as exc:
            raise RuntimeError('Baseline comparisons require the original Git history. '
                'Use a full clone, or run: git fetch https://github.com/brad-colbert/ansivbxe.git '+COMMIT) from exc
        if digest(data)!=item['sha']:raise ValueError(f'Baseline hash mismatch: {path}')
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(data)
        dest.chmod(0o755 if item['mode']=='100755' else 0o644)
    return src

def build():
    src=materialize();out=ROOT/'baseline/compiled';out.mkdir(exist_ok=True)
    def run(*cmd):subprocess.run(cmd,cwd=src,check=True)
    run('ca65','-g','ANSIVBXE_ca65.asm','-o',str(out/'main.o'))
    run('ca65','-g','vbxe_lib.asm','-o',str(out/'vbxe_lib.o'))
    run('ld65','-C',str(ROOT/'vbxe-plus.cfg'),'-S','0x2800','-D','start=0x2800','-Ln',str(out/'symbols.lbl'),'--dbgfile',str(out/'program.dbg'),'-m',str(out/'program.map'),str(out/'main.o'),str(out/'vbxe_lib.o'),'-o',str(out/'PRISM.XEX'))
if __name__=='__main__':build()
