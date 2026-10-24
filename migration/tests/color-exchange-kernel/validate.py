#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess,argparse,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'results.json').read_text());assert not r['changed_sources'];assert len(r['source_sha256'])==7
with tarfile.open(E/'sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():assert sha(t.extractfile(n).read())==h,n
for n,h in r['source_sha256'].items():
 if a.check_source:assert sha((R/n).read_bytes())==h,n
for mode,row in r['runs'].items():assert row['returncode']==0 and 'KERNEL_COMPARISONS 989864'in row['stdout']and 'PASS 17387044'in row['stdout'];assert '-fno-rtti'in row['commands'][1]and '-std=c++14'in row['commands'][1]
assert set(r['runs'])=={'normal','sanitizer'}and '-fsanitize=address,undefined,float-cast-overflow'in r['runs']['sanitizer']['commands'][1]
assert {x['name']for x in r['negative_controls']}=={'exclusive-threshold','partial-match','signed-delta','no-saturation','alpha-overwrite'}
for x in r['negative_controls']:assert x['returncode']!=0 and 'Assertion'in x['stderr']
old=subprocess.check_output(['git','show','afa43fae:libgimp/gimp.h'],cwd=R);assert b'guint8            d_int8;'in old
subprocess.run(['python3','-B',str(R/'migration/tests/color-exchange-foundation/validate.py')],check=True)
print('PASS typed ColorExchange kernel989,864 normal + sanitizer comparisons;5 header mutants; no native route admission')
