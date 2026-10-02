#!/usr/bin/env python3
"""Execute unchanged pinned native layer-preset code. Run under build lock with old env."""
import gzip,hashlib,json,os,shlex,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[2]; old=root.parent/'gimp-painter-legacy'
out=root/'migration/fixtures/legacy-layer-presets'; commit='afa43fae3e920210146abed514f136fd49f671b5'
source=old/'app/presets/layer-preset.cpp'
assert source.read_bytes()==subprocess.check_output(['git','-C',str(old),'show',commit+':app/presets/layer-preset.cpp'])
flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','gtk+-2.0','gegl-0.3','json-glib-1.0'],text=True))
prior=json.loads((root/'migration/fixtures/legacy-fill-brush/runtime.json').read_text())
line=prior['link_command']
report={'kind':'actual pinned native LayerPresetApplier construction','commit':commit,'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'scope':'All eight exact bundled JSONs; native construction and typed arguments; no interactive UI claim'}
with tempfile.TemporaryDirectory(prefix='preset-oracle-') as tmp:
 work=Path('/tmp/gimp-layer-preset-oracle');work.mkdir(exist_ok=True);obj=work/'capture.o';binary=work/'capture'
 compile=['c++','-std=gnu++14','-g','-O2','-include','type_traits','-I'+str(old),'-I'+str(old/'app'),'-I'+str(old/'app/tests'),*flags,'-c',str(out/'capture.cpp'),'-o',str(obj)]
 result=subprocess.run(compile,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(out/'runtime-build.log').write_bytes(result.stdout)
 if result.returncode: raise SystemExit(result.stdout.decode())
 import re
 link=re.sub(r'-o /tmp/[^ ]+/capture /tmp/[^ ]+/capture.o',f'-o {binary} {obj}',line)
 result=subprocess.run(link,shell=True,cwd=old/'app/tests',stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
 with (out/'runtime-build.log').open('ab') as f:f.write(result.stdout)
 if result.returncode: raise SystemExit(result.stdout.decode())
 env=os.environ.copy();env['GIMP_TESTING_ABS_TOP_SRCDIR']=str(old);env['GIMP_TESTING_ABS_TOP_BUILDDIR']=str(old);env['UI_TEST']='yes'
 run=subprocess.run([str(binary),str(old/'data/layer-presets')],cwd=old/'app/tests',env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=120)
 with gzip.open(out/'runtime-capture.log.gz','wb') as f:f.write(run.stdout)
 (out/'runtime-capture.stderr').write_bytes(run.stderr)
 lines=[l for l in run.stdout.decode().splitlines() if l.startswith(('PRESET','LAYER_PRESET_CAPTURE_COMPLETE'))]
 (out/'construction.tsv').write_text('\n'.join(lines)+'\n')
 report.update(exit_code=run.returncode,compile_command=compile,link_command=link,binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),archive_sha256=hashlib.sha256((old/'app/presets/libapppresets.a').read_bytes()).hexdigest())
 (out/'runtime.json').write_text(json.dumps(report,indent=2)+'\n')
 print('\n'.join(lines));print(run.stderr.decode());print('exit',run.returncode)
 if run.returncode: raise SystemExit(run.returncode)
