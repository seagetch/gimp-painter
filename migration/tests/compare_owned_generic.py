#!/usr/bin/env python3
"""Compare actual generic port APIs against pinned old native entrypoints."""
import argparse,gzip,hashlib,json,os,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('build',type=Path);p.add_argument('--report',type=Path,required=True);a=p.parse_args()
root=Path(__file__).resolve().parents[2];build=a.build.resolve();runs=[]
env={**os.environ,'GIMP_TESTING_ABS_TOP_SRCDIR':str(root),'GIMP_TESTING_ABS_TOP_BUILDDIR':str(build),'GIMP_TESTING_PLUGINDIRS':str(build/'plug-ins/common'),'UI_TEST':'yes','GIMP3_DIRECTORY':str(build/'app/tests/gimpdir-output')}
for family,prefix in [('fill-brush',b'BRUSH '),('smudge',b'SMUDGE ')]:
 for route in ['raw','path','boundary']:
  exe=build/'app/tests'/('painter-'+family+'-trace')
  run=subprocess.run([str(exe)],env={**env,'PAINTER_OWNED_GENERIC_ROUTE':route},stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  actual=b'\n'.join(x for x in run.stdout.splitlines() if x.startswith(prefix) or x.endswith(b'_CAPTURE_COMPLETE'))+b'\n'
  expected=gzip.decompress((root/'migration/fixtures/legacy-owned-generic'/(family+'-'+route+'.tsv.gz')).read_bytes())
  record={'family':family,'route':route,'exit_code':run.returncode,'bytes':len(actual),'records':len(actual.splitlines()),'exact':actual==expected,'stderr':run.stderr.decode(),'sha256':hashlib.sha256(actual).hexdigest(),'executable_sha256':hashlib.sha256(exe.read_bytes()).hexdigest()}
  if actual!=expected:
   record['differences']=[{'record':str(e.split()[:3]),'different_hex_characters':sum(x!=y for x,y in zip(e,b))} for e,b in zip(expected.splitlines(),actual.splitlines()) if e!=b]
  runs.append(record);print(f'{family}/{route}: exit={run.returncode}, exact={record["exact"]}',flush=True)
a.report.write_text(json.dumps({'scope':'Real generic port entrypoints versus independent pinned old Stroke/Vectors/Boundary fixtures','runs':runs},indent=2)+'\n')
assert all(x['exit_code']==0 and x['exact'] for x in runs)
