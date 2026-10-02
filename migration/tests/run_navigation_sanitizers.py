#!/usr/bin/env python3
"""Focused ASan/UBSan of the pure display-input math; no GUI coverage."""
import argparse, hashlib, json, os, pathlib, shlex, subprocess, tempfile
p=argparse.ArgumentParser();p.add_argument('--build-dir',default='build-debian13');args=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];build=root/args.build_dir
sources=['app/display/gimppainternavigation.c','app/tests/test-painter-navigation.c']
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gtk+-3.0','cairo'],text=True))
with tempfile.TemporaryDirectory(prefix='painter-navigation-sanitize-') as tmp:
 exe=str(pathlib.Path(tmp)/'navigation')
 command=['cc','-std=c11','-g','-O1','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',f'-I{build}',f'-I{root}/app',*[str(root/s) for s in sources],*flags,'-lm','-o',exe]
 compile_result=subprocess.run(command,capture_output=True,text=True)
 if compile_result.returncode: raise RuntimeError(compile_result.stderr)
 env=os.environ.copy();env['ASAN_OPTIONS']='detect_leaks=0:abort_on_error=1';env['UBSAN_OPTIONS']='halt_on_error=1:print_stacktrace=1'
 result=subprocess.run([exe,str(root/'migration/fixtures/legacy-navigation/rotation.tsv'),str(root/'migration/fixtures/legacy-zoom/zoom.tsv')],env=env,capture_output=True,text=True)
 report={'scope':'pure navigation math and six test groups; GTK/Cairo uninstrumented; no GUI/device run','sanitizers':['address','undefined'],'leak_detection':False,'source_sha256':{s:hashlib.sha256((root/s).read_bytes()).hexdigest() for s in sources},'compile_command':command,'returncode':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
 (root/'migration/tests/navigation-sanitizers.json').write_text(json.dumps(report,indent=2)+'\n')
 print(result.stdout,end='');print(result.stderr,end='');raise SystemExit(result.returncode)
