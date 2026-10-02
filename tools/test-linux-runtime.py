#!/usr/bin/env python3
"""Verify a staged relocatable runtime without any developer dependency paths.

Run under the shared build lock. This automates ELF resolution, version and
console create/fill/save/reopen; it never claims actual GUI or pen coverage.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess


BASE_ABI = {'ld-linux-x86-64.so.2', 'libc.so.6', 'libm.so.6', 'libmvec.so.1',
            'libpthread.so.0', 'librt.so.1', 'libdl.so.2', 'libutil.so.1',
            'libresolv.so.2'}


def require_base_abi(soname):
    if soname not in BASE_ABI:
        raise RuntimeError(f'Non-baseline host dependency must be bundled: {soname}')


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source, output = args.bundle.resolve(), args.output.resolve()
    if output.exists():
        parser.error('Output must be new')
    output.mkdir(parents=True)
    relocated = output/'relocated path 日本語'/source.name
    shutil.copytree(source, relocated, symlinks=True)
    profile = output/'profile'
    for name in ['home','config','cache','data']:
        (profile/name).mkdir(parents=True)
    env = {'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','HOME':str(profile/'home'),
           'XDG_CONFIG_HOME':str(profile/'config'),'XDG_CACHE_HOME':str(profile/'cache'),
           'XDG_DATA_HOME':str(profile/'data'),'GIMP_BASELINE_XCF':str(output/'smoke.xcf')}
    app = relocated/'AppRun'
    check = json.loads((source/'file-manifest.json').read_text())
    verified = 0
    for rel, item in check.items():
        path = relocated/rel
        if 'symlink' in item:
            assert path.is_symlink() and os.readlink(path) == item['symlink'], rel
            assert path.resolve().is_relative_to(relocated), rel
        else:
            assert path.is_file() and digest(path) == item['sha256'], rel
            assert oct(path.stat().st_mode & 0o777) == item['mode'], rel
        verified += 1
    manifest = json.loads((source/'build-manifest.json').read_text())
    for soname in manifest['host_runtime']:
        require_base_abi(soname)
    libs = [relocated/'usr/lib/x86_64-linux-gnu', relocated/'deps/usr/lib/x86_64-linux-gnu']
    libs.extend([libs[1]/'blas', libs[1]/'lapack'])
    ld_env = {**env,'LD_LIBRARY_PATH':':'.join(str(p) for p in libs)}
    dependencies, checked_elf = {}, 0
    for file in sorted(relocated.rglob('*')):
        if file.is_symlink() or not file.is_file():
            continue
        with file.open('rb') as stream:
            if stream.read(4) != b'\x7fELF':
                continue
        completed = subprocess.run(['ldd',str(file)],env=ld_env,capture_output=True,text=True)
        assert completed.returncode == 0 and 'not found' not in completed.stdout, str(file)+'\n'+completed.stdout+completed.stderr
        rows = []
        for line in completed.stdout.splitlines():
            match = re.match(r'\s*(\S+) => (/.+?) \(',line)
            if match:
                name, target = match.groups()
                inside = Path(target).is_relative_to(relocated)
                if not inside:
                    require_base_abi(name)
                    assert name in manifest['host_runtime'], name+': '+target
                rows.append({'soname':name,'path':str(Path(target).relative_to(relocated)) if inside else target,'bundled':inside})
        dependencies[str(file.relative_to(relocated))]=rows
        checked_elf += 1
    trace_available = False
    trace_unavailable_reason = 'strace executable not installed'
    if shutil.which('strace'):
        probe = subprocess.run(['strace','-qq','-o',str(output/'strace-probe.log'),'/bin/true'],
                               env=env, capture_output=True, text=True)
        trace_available = probe.returncode == 0
        trace_unavailable_reason = None if trace_available else probe.stderr.strip()
    runs = []
    for name, argv, stdin in [
        ('version',[app,'--version'],None),
        ('console-create-fill-save-reopen',[app,'--console','-n','-c','--batch-interpreter=python-fu-eval','-b','-','--quit'],
         Path(__file__).resolve().parent.parent/'migration/tests/baseline-smoke.py')]:
        with (output/(name+'.log')).open('w') as log:
            result = subprocess.run((['strace','-f','-qq','-e','trace=file','-o',str(output/(name+'-files.trace'))] if trace_available else []) + [str(a) for a in argv],env=env,
                input=stdin.read_text() if stdin else None,text=True,stdout=log,stderr=subprocess.STDOUT,timeout=180)
        contents = (output/(name+'.log')).read_text()
        passed = result.returncode == 0
        if stdin:
            passed &= 'BASELINE_CREATE_FILL_SAVE_REOPEN_OK' in contents
        trace_file = output/(name+'-files.trace')
        forbidden_hits = []
        if trace_file.exists():
            forbidden = [str(Path(__file__).resolve().parent.parent), '/workspace/shared/gimp-build-deps', '.build-prefix-debian13']
            forbidden_hits = [line for line in trace_file.read_text().splitlines() if any(path in line for path in forbidden)]
            passed &= not forbidden_hits
        runs.append({'name':name,'exit_code':result.returncode,'passed':passed,'log':name+'.log',
                     'file_trace_available':trace_file.exists(),'build_path_accesses':forbidden_hits})
        if not passed:
            print(contents)
    report={'format':1,'source_commit':manifest['source_commit'],'prototype':manifest['status'],
        'bundle_manifest_sha256':digest(source/'build-manifest.json'),
        'verified_files':verified,'elf_objects':checked_elf,'relocation':'fresh path with spaces and Japanese characters',
        'environment':'explicit minimum whitelist; no build/dependency tree variables',
        'resource_syscall_audit':{'available':trace_available,'unavailable_reason':trace_unavailable_reason},
        'runs':runs,'dependencies':dependencies,'all_passed':all(r['passed'] for r in runs),
        'not_covered':['native GUI drawing','X11 pen','Wayland','Windows','macOS','physical tablet','full legacy corpus']}
    if (output/'smoke.xcf').exists():
        report['xcf']={'bytes':(output/'smoke.xcf').stat().st_size,'sha256':digest(output/'smoke.xcf')}
    (output/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='dependencies'},indent=2))
    raise SystemExit(0 if report['all_passed'] else 1)


if __name__ == '__main__':
    main()
