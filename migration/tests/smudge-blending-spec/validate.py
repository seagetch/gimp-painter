#!/usr/bin/env python3
import hashlib,json,pathlib,subprocess,sys
p=pathlib.Path(__file__).resolve().parent
for n,h in json.loads((p/'artifact-sha256.json').read_text()).items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
assert 'Ran 7 tests' in(p/'tests.log').read_text() and(p/'tests.log').read_text().rstrip().endswith('OK')
subprocess.run([sys.executable,str(p/'test_spec.py')],check=True)
print('PASS original23.001 specification:52 original duties,1024 old two-stage vectors,independent Rate/Flow/Blending and byte quirks')
