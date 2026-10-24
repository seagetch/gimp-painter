#!/usr/bin/env python3
"""Portable integrity and bounded-result checks; does not execute GIMP."""
import hashlib,json,re,tarfile
from pathlib import Path
P=Path(__file__).resolve().parent
CASES=['filter_red_eye_native_basic','red_eye_thresholds_and_rejections','red_eye_unsupported_formats','red_eye_component_masks','red_eye_late_context_routes','red_eye_cancel_and_resume','red_eye_invalid_arguments','filter_gaussian_attributes_contract']
def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,msg):
 if not ok:raise ValueError(msg)
def result(j,expected):
 check(j['exit_code']==0,'failed result')
 check(re.findall(r'^ok \d+ (\S+)',j['stdout'],re.M)==['/gimp-filter-layer/'+x for x in expected],'wrong cases')
def main():
 a=json.loads((P/'artifact-sha256.json').read_text())
 check(set(a)=={str(p.relative_to(P)) for p in P.rglob('*') if p.is_file() and p.name!='artifact-sha256.json' and '__pycache__' not in p.parts},'artifact set')
 for p,h in a.items():check(sha((P/p).read_bytes())==h,p)
 n=json.loads((P/'native.json').read_text());s=json.loads((P/'sanitizer.json').read_text());b=json.loads((P/'source-before.json').read_text());e=json.loads((P/'source-after.json').read_text())
 check(set(n)==set(CASES),'native set')
 for p,j in n.items():result(j,[p])
 result(s,CASES)
 check(not s['changed_after_compile'] and not e['changed_after_run'] and b['sha256']==e['sha256'],'source drift')
 check(len(s['instrumented_sources'])==23 and s['leak_detection'] is False and s['instrumented_cpp_rtti'] is True,'instrumentation scope')
 expected=dict(s['source_sha256']);expected.update(b['sha256']);actual={}
 with tarfile.open(P/'sources.tar.gz') as t:
  for f in t:
   check(f.isfile() and not Path(f.name).is_absolute() and '..' not in Path(f.name).parts and f.name not in actual,'archive member')
   actual[f.name]=sha(t.extractfile(f).read())
 check(actual==expected,'source archive')
 for p,h in json.loads((P/'owned-source-sha256.json').read_text()).items():check(actual[p]==h,'owned source')
 red=json.loads((P/'red/result.json').read_text());check(red['exit_code']!=0 and 'Saved filter procedure or argument mapping is unsupported' in red['stdout'],'missing causal red')
 first=json.loads((P/'fixture-failure/native.json').read_text());check(first['red_eye_invalid_arguments']['exit_code']!=0 and 'gimp_value_array_truncate' in first['red_eye_invalid_arguments']['stdout'],'missing fixture failure')
 for mutant in (dict(s,exit_code=1),dict(s,stdout=s['stdout'].replace('/filter_red_eye_native_basic','/wrong')),dict(s,stdout=s['stdout'].replace('ok 1 /gimp-filter-layer/filter_red_eye_native_basic','not ok 1 /gimp-filter-layer/filter_red_eye_native_basic'))):
  try:result(mutant,CASES)
  except ValueError:pass
  else:raise ValueError('failed negative control')
 print(f"PASS native8/san8;{len(actual)} source seals;23 instrumented/{len(s['rtti_compatibility_only_sources'])} RTTI-only;causal red and retained fixture correction;3 negative controls")
if __name__=='__main__':main()
