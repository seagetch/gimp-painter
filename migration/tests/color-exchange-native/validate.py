#!/usr/bin/env python3
"""Verify bounded native Exchange receipts without executing GIMP."""
from pathlib import Path
import argparse,hashlib,json,re,tarfile,subprocess
P=Path(__file__).resolve().parent;R=P.parents[2]
OWN=['app/core/gimpfilterlayer.cpp','app/tests/test-gimp-filter-layer.c','app/tests/test-filter-color-exchange-contract.inc','migration/tests/run_color_exchange_native_sanitizers.py']
CASES=['filter_color_exchange_native_basic','exchange_unsupported_formats','exchange_component_masks','exchange_late_context_routes','exchange_cancel_and_resume','exchange_invalid_arguments','exchange_outside_selection_no_merge','exchange_rgb_rgba_inputs','exchange_noalpha_owner_rejected','exchange_projection_extent_cache']
REGRESSIONS=['filter_red_eye_native_basic','red_eye_component_masks','red_eye_invalid_arguments','filter_gaussian_attributes_contract']
def sha(b):return hashlib.sha256(b).hexdigest()
def check(ok,msg):
 if not ok:raise ValueError(msg)
def result(j,expected):
 check(j['exit_code']==0,'failed result')
 check(re.findall(r'^ok \d+ (\S+)',j['stdout'],re.M)==['/gimp-filter-layer/'+x for x in expected],'wrong test cases')
def main():
 p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');args=p.parse_args()
 artifacts=json.loads((P/'artifact-sha256.json').read_text())
 check(set(artifacts)=={str(p.relative_to(P))for p in P.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts},'artifact set')
 for n,h in artifacts.items():check(sha((P/n).read_bytes())==h,n)
 native=json.loads((P/'native.json').read_text());san=json.loads((P/'sanitizer.json').read_text());before=json.loads((P/'source-before.json').read_text());after=json.loads((P/'source-after.json').read_text())
 check(set(native)==set(CASES+REGRESSIONS),'native case set')
 correction=json.loads((P/'scope-correction.json').read_text());check(correction['sha256']==sha((P/'source-before.json').read_bytes()) and before['raw_committed']=='e1226f62de' and len(before['raw_paths'])==6,'scope clarification')
 for name,j in native.items():result(j,[name])
 result(san,CASES);check(len(san['sources'])==9 and not san['instrumented_cpp_rtti'] and not san['leak_detection'],'instrumentation scope')
 check(not san['changed_after_run'] and not after['changed_after_run'] and before['sha256']==after['sha256'],'source drift')
 expected=dict(before['sha256']);expected.update(san['source_sha256']);actual={}
 with tarfile.open(P/'sources.tar.gz')as t:
  for f in t:
   check(f.isfile()and not Path(f.name).is_absolute()and '..'not in Path(f.name).parts and f.name not in actual,'archive path')
   actual[f.name]=sha(t.extractfile(f).read())
 check(actual==expected,'source archive')
 if args.check_source:
  for n in OWN+before['raw_paths']:check(sha((R/n).read_bytes())==actual[n],n)
 red=json.loads((P/'red/result.json').read_text());check(red['exit_code']!=0 and 'Saved filter procedure or argument mapping is unsupported'in red['stdout'],'missing causal red')
 failure=json.loads((P/'owner-fixture-failure/native.json').read_text())['exchange_rgb_rgba_owners'];check(failure['exit_code']!=0 and 'Filter context requires an alpha-bearing RGB or Gray layer'in failure['stdout'],'owner policy evidence')
 exploratory=json.loads((P/'mutations-exploratory.json').read_text());check(exploratory[-1]['name']=='omit-owner-projection' and exploratory[-1]['exit_code']==0,'preserved in-canvas survivor')
 mutants=json.loads((P/'mutations.json').read_text());check({m['name']for m in mutants}=={'omit-uint8-domain','wrong-to-slots','omit-start-roi-binding','omit-owner-projection-extent'},'mutant set')
 for m in mutants:check(m['exit_code']!=0 and m['source_sha256']==actual['app/core/gimpfilterlayer.cpp'],'mutation result')
 result(json.loads((P/'restored.json').read_text()),['filter_color_exchange_native_basic','exchange_invalid_arguments','exchange_projection_extent_cache'])
 audit=json.loads((P/'type-audit.json').read_text());check(audit['unmodified_outside_new_branch'],'core scope')
 with tarfile.open(P/'sources.tar.gz')as t:s=t.extractfile('app/core/gimpfilterlayer.cpp').read().decode()
 a=s.index('    else if (procedure == "plug-in-exchange")');z=s.index('    else if (procedure == "plug-in-red-eye-removal")',a)
 check(sha((s[:a]+s[z:]).encode())==audit['head_core_sha256'],'changed unrelated core provider')
 # Independent result-admission negative controls, never execute application.
 for bad in [dict(san,exit_code=1),dict(san,stdout=san['stdout'].replace('/filter_color_exchange_native_basic','/wrong')),dict(san,stdout=san['stdout'].replace('ok 1 ','not ok 1 ',1))]:
  try:result(bad,CASES)
  except ValueError:pass
  else:raise ValueError('negative result admitted')
 subprocess.run(['python3','-B',str(R/'migration/tests/color-exchange-shadow/validate.py')],check=True)
 print(f'PASS Exchange native14, selected9-unit sanitizer10,4 causal mutants,3 result controls,{len(actual)} source seals; XCF/UI separate')
if __name__=='__main__':main()
