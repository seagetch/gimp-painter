#!/usr/bin/env python3
"""Run exact-source native Filter checks, optionally reusing a proven ASan build.

Hold the shared lock. A separated large case is reported separately, never
silently counted as part of a completed full-suite process.
"""
import argparse, hashlib, json, os, subprocess, time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True)
p.add_argument('--sanitizer-build',type=Path);p.add_argument('--mode',choices=['foundation','other82','large','all'],default='foundation')
a=p.parse_args();root=Path(__file__).resolve().parents[2];build=a.build.resolve()
sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
source_names=['app/core/gimpfilterlayer.cpp','app/core/gimpfilterlayer.h','app/core/gimpfilterlayer-arguments.hpp',
'app/painter/filter-scheduler.cpp','app/painter/filter-scheduler.hpp','app/painter/filter-spool.cpp','app/painter/filter-spool.hpp',
'app/painter/filter-raster.cpp','app/painter/filter-raster.hpp','app/painter/filter-native-kernels.cpp','app/painter/filter-native-kernels.hpp',
'app/painter/filter-edge-kernel-private.hpp','app/painter/filter-gauss-kernel-private.hpp','app/painter/binding-store.cpp',
'app/painter/binding-store.hpp','app/tests/test-gimp-filter-layer.c','app/tests/test-gimp-filter-layout.cpp']
proof=None
if a.sanitizer_build:
    proof=json.loads(a.sanitizer_build.read_text());source_names=list(proof['source_sha256'])
    if proof.get('changed_after_compile'):raise SystemExit('Sanitizer source changed during compile')
    if any(sha(root/n)!=h for n,h in proof['source_sha256'].items()):raise SystemExit('Sanitizer source proof is stale')
before={n:sha(root/n) for n in source_names}
exe=build/'app/tests'/('gimp-filter-layer-asan' if proof else 'gimp-filter-layer')
cmd=[str(exe)]
if a.mode=='foundation':cmd+=['-p','/gimp-filter-layer/image_close_during_worker','-p','/gimp-filter-layer/retained_handle_after_image_close']
elif a.mode=='other82':cmd+=['-s','/gimp-filter-layer/large_spill_execution_finishes']
elif a.mode=='large':cmd+=['-p','/gimp-filter-layer/large_spill_execution_finishes']
env=dict(os.environ);env.update(GIMP_TESTING_ABS_TOP_SRCDIR=str(root),GIMP_TESTING_ABS_TOP_BUILDDIR=str(build),
    GIMP_TESTING_PLUGINDIRS=str(build/'plug-ins/common'),UI_TEST='yes')
if proof:env.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1:abort_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
exesha=sha(exe);started=time.monotonic();result=subprocess.run(cmd,cwd=build,env=env,capture_output=True,text=True,timeout=180)
report=dict(scope='Actual native FilterLayer '+a.mode,source_sha256=before,command=cmd,executable_sha256=exesha,
    exit_code=result.returncode,stdout=result.stdout,stderr=result.stderr,elapsed_seconds=time.monotonic()-started,
    changed_after_run=[n for n,h in before.items() if sha(root/n)!=h],
    skipped=['/gimp-filter-layer/large_spill_execution_finishes'] if a.mode=='other82' else [],
    sanitizers=proof['sanitizers'] if proof else [],leak_detection=False)
if proof:
    report.update(sanitizer_build_report=str(a.sanitizer_build),sanitizer_build_sha256=sha(a.sanitizer_build),
        instrumented_sources=proof['instrumented_sources'],rtti_compatibility_only_sources=proof['rtti_compatibility_only_sources'])
a.report.write_text(json.dumps(report,indent=2)+'\n');print(result.stdout);print(result.stderr if result.returncode else '')
raise SystemExit(result.returncode or bool(report['changed_after_run']))
