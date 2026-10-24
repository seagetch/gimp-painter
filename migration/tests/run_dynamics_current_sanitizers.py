#!/usr/bin/env python3
"""Existing five-C-unit model sanitizer plus real legacy-neutral curve entry.
Hold private build lock. Geometry driver/remaining host are uninstrumented.
"""
import argparse,pathlib,subprocess,json,hashlib,shlex,os
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--report',required=True,type=pathlib.Path);a=p.parse_args();R=pathlib.Path(__file__).resolve().parents[2];B=a.build.resolve();O=a.report.parent;O.mkdir(parents=True,exist_ok=True)
inputs=['app/core/gimpdynamics.c','app/core/gimpdynamicsoutput.c','app/core/gimpcurve.c','app/core/core-enums.c','app/tests/test-painter-dynamics.c','app/tests/test-painter-brush-geometry.cpp','app/paint/gimppaintoptions.c','migration/fixtures/legacy-brush-geometry/default-dynamics.tsv.gz','migration/tests/run_dynamics_sanitizers.py','migration/tests/run_dynamics_current_sanitizers.py'];hashes={n:hashlib.sha256((R/n).read_bytes()).hexdigest()for n in inputs}
subprocess.run(['python3',str(R/'migration/tests/run_dynamics_sanitizers.py'),str(B),'--report',str(O/'five-c-units.json')],check=True)
base=json.loads((O/'five-c-units.json').read_text());assert base['exit_code']==0 and not base['changed_during_build']
cmd=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-brush-geometry'],cwd=B,text=True).strip().splitlines()[-1]);exe=B/'app/tests/painter-brush-geometry-neutral-asan';cmd[cmd.index('-o')+1]=str(exe)
for i,arg in enumerate(cmd):
 replacement=B/'dynamics-sanitizers'/arg.replace('/','_')
 if arg.endswith('.a')and replacement.is_file():cmd[i]=str(replacement)
cmd[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-O1'];subprocess.run(cmd,cwd=B,check=True)
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(R),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(B),'GIMP_TESTING_PLUGINDIRS':str(B/'plug-ins/common'),'UI_TEST':'yes','ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1:abort_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'}
run=subprocess.run([str(exe),'-p','/painter/geometry/default-dynamics'],cwd=B,env=env,capture_output=True,text=True,timeout=60)
report={'scope':'Existing five C units instrumented; same model/output/curve/enum replacement archives in actual registered neutral mapping path. Geometry C++ test driver/remaining host and dependencies uninstrumented; LSan disabled.','sources_sha256':hashes,'changed_during_run':[n for n,h in hashes.items()if hashlib.sha256((R/n).read_bytes()).hexdigest()!=h],'link_command':cmd,'five_c_report_sha256':hashlib.sha256((O/'five-c-units.json').read_bytes()).hexdigest(),'five_c_executable_sha256':hashlib.sha256((B/'app/tests/painter-dynamics-asan').read_bytes()).hexdigest(),'geometry_executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest(),'exit_code':run.returncode,'stdout':run.stdout,'stderr':run.stderr};a.report.write_text(json.dumps(report,indent=2)+'\n');print(run.stdout);assert run.returncode==0 and not report['changed_during_run']
