from pathlib import Path
import subprocess,json,hashlib,os
O=Path(__file__).resolve().parent;p=O/'overlay/app/display/gimpdisplayshell-tool-events.c';raw=p.read_bytes();s=raw.decode();a='if (! active &&\n      compression_generation == shell->painter_compression_generation)';assert s.count(a)==1
mut=s.replace(a,'if (! active)');(O/'generation-mutant-provider.c').write_text(mut)
try:
 p.write_text(mut)
 with (O/'build-generation-mutant.log').open('w') as f:subprocess.run(['flock','/workspace/shared/gimp-clean-aggregate/build.lock','python3',str(O/'build-v2.py')],stdout=f,stderr=subprocess.STDOUT,check=True)
 (O/'generation-mutant-build.json').write_bytes((O/'build.json').read_bytes());(O/'generation-mutant-link.json').write_bytes((O/'link-before.json').read_bytes())
finally:p.write_bytes(raw)
subprocess.run(['python3',str(O/'run.py'),'generation-mutant','-p','/event-order/nested-new-stroke'],check=True)
r=json.loads((O/'generation-mutant.json').read_text());assert r['returncode']!=0 and "'shell->painter_compression_window==w' should be TRUE" in r['stderr'],r['stderr'][-1000:]
print('PASS causal mutant rejected: failed outer callback must not release newer nested lease')
