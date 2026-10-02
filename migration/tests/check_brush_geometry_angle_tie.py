#!/usr/bin/env python3
"""Reproduce the negative half-millidegree-axis finding on current native code.
Hold the shared build lock and source the dependency environment. This compiles
only the probe and links existing production archives; no production rebuild.
"""
import argparse,gzip,hashlib,json,os,shlex,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();fixture=root/'migration/fixtures/legacy-brush-geometry-angle-tie';out=build/'brush-geometry-angle-probe';out.mkdir(exist_ok=True)
entry=next(e for e in json.loads((build/'compile_commands.json').read_text()) if e['file'].endswith('/painter-brush-geometry-trace.cpp'))
cmd=shlex.split(entry['command']);cmd[cmd.index('-o')+1]=str(out/'trace.o');cmd[-1]=str(fixture/'modern-probe.cpp')
subprocess.run(cmd,cwd=build,check=True)
link=shlex.split(subprocess.check_output(['ninja','-t','commands','app/tests/painter-brush-geometry-trace'],cwd=build,text=True).strip().splitlines()[-1]);link[link.index('-o')+1]=str(out/'trace');link=[str(out/'trace.o') if arg==entry['output'] else arg for arg in link]
subprocess.run(link,cwd=build,check=True)
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GIMP3_DIRECTORY':str(build/'geometry-native-profile')}
run=subprocess.run([str(out/'trace')],env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
actual=b'\n'.join(line for line in run.stdout.splitlines() if line.startswith(b'GEOMETRY_'))+b'\n';expected=gzip.decompress((fixture/'pixels.tsv.gz').read_bytes())
report={'scope':'Normal-only extra144 actual-old generated strokes plus exact native axes at angle0.0625 degrees; no production correction required','exit_code':run.returncode,'exact':actual==expected,'records':len(expected.splitlines()),'old_axes':expected.splitlines()[0].decode(),'new_axes':actual.splitlines()[0].decode(),'binary_sha256':hashlib.sha256((out/'trace').read_bytes()).hexdigest(),'harness_sha256':hashlib.sha256((fixture/'modern-probe.cpp').read_bytes()).hexdigest(),'expected_sha256':hashlib.sha256(expected).hexdigest(),'actual_sha256':hashlib.sha256(actual).hexdigest(),'compile_command':cmd,'link_command':link,'stderr':run.stderr.decode()}
a.report.write_text(json.dumps(report,indent=2)+'\n');print('Exact old half-angle axes/strokes',report['exact'],report['records'])
if run.returncode or actual!=expected:raise SystemExit(1)
