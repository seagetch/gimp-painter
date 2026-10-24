#!/usr/bin/env python3
"""Validate exact bounded Video private-native receipts, no application run."""
from pathlib import Path
import argparse,hashlib,json,re,tarfile,subprocess
P=Path(__file__).resolve().parent;R=P.parents[2]
CASES=['filter_video_native_basic','video_native_patterns_and_flags','video_native_roi_phase','video_native_invalid_and_formats','video_native_rgb_rgba_inputs','video_native_noalpha_owner_rejected','video_native_cancel_and_resume','video_native_invalid_arguments','video_native_late_context_routes','video_native_component_masks']
REG=['filter_red_eye_native_basic','filter_colorify_native_basic','filter_color_exchange_native_basic','filter_normalize_native_basic']
def sha(b):return hashlib.sha256(b).hexdigest()
def check(b,m):
 if not b:raise ValueError(m)
def result(r,case):
 check(r['exit_code']==0,'failed result')
 check(re.findall(r'^ok \d+ (\S+)',r['stdout'],re.M)==['/gimp-filter-layer/'+case],'wrong TAP case')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check-source',action='store_true');args=ap.parse_args()
 artifacts=json.loads((P/'artifact-sha256.json').read_text());actual_files={str(p.relative_to(P))for p in P.rglob('*')if p.is_file()and p.name!='artifact-sha256.json'and '__pycache__'not in p.parts};check(set(artifacts)==actual_files,'artifact set')
 for p,h in artifacts.items():check(sha((P/p).read_bytes())==h,p)
 results=json.loads((P/'results.json').read_text());seal=json.loads((P/'seal.json').read_text());owned=json.loads((P/'owned.json').read_text());sources=json.loads((P/'source-sha256.json').read_text());scope=json.loads((P/'scope.json').read_text());baseline=json.loads((P/'baseline-source-sha256.json').read_text())
 check(set(results)=={'normal','sanitizer'},'modes')
 for mode,cases in [('normal',CASES+REG),('sanitizer',CASES)]:
  check(set(results[mode]['runs'])==set(cases),'case set')
  for case,r in results[mode]['runs'].items():result(r,case)
  for n,h in results[mode]['sources'].items():check(sources[n]==h,'instrumented source')
 check(len(results['sanitizer']['sources'])==10 and scope['instrumented_units']==10 and scope['cpp_units_no_rtti']==7 and scope['cpp_units_default_rtti']==2 and not scope['vptr_sanitizer'] and not scope['leak_detection'],'sanitizer scope')
 commands=[c for c in results['sanitizer']['commands']if '-c'in c]
 check(len(commands)==10 and all('-fsanitize=address,undefined'in c and '-fno-sanitize=vptr'in c for c in commands),'actual sanitizer compiler flags')
 cpp=[c for c in commands if any(x.endswith('.cpp')for x in c)]
 check(len(cpp)==9 and sum('-fno-rtti'in c for c in cpp)==7 and sum('-fno-rtti'not in c for c in cpp)==2,'actual RTTI compiler scope')
 check(seal['base_commit']=='55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94' and not seal['changed'] and seal['owned']==owned,'exact baseline/overlay')
 check(sha((P/'baseline-source-sha256.json').read_bytes())==seal['manifest_sha256'] and len(baseline)==seal['base_sources'],'baseline manifest')
 check(len(json.loads((P/'linked-before.json').read_text()))==seal['linked_inputs'],'linked count')
 archived={};data={}
 with tarfile.open(P/'sources.tar.gz')as t:
  for f in t:
   check(f.isfile()and not Path(f.name).is_absolute()and '..'not in Path(f.name).parts and f.name not in archived,'archive path');data[f.name]=t.extractfile(f).read();archived[f.name]=sha(data[f.name])
 check(archived==sources,'source archive')
 for n,h in owned.items():check(sources[n]==h,'owned archive')
 if args.check_source:
  for n in list(owned):
   if n not in {'app/tests/test-gimp-filter-layer.c','app/tests/test-filter-video-contract.inc'}:check(sha((R/n).read_bytes())==sources[n],n)
 core=data['app/core/gimpfilterlayer.cpp'].decode();a=core.index('    else if (procedure == "plug-in-video")');z=core.index('    else if (procedure == "plug-in-normalize")',a);old=(core[:a]+core[z:]);check(sha(old.encode())==scope['core_before_sha256'] and scope['only_new_video_branch'],'unrelated core drift')
 red=json.loads((P/'expected-red.json').read_text());check(red['exit_code']!=0 and 'Saved filter procedure or argument mapping is unsupported'in red['stdout'],'missing-route red')
 mutations=json.loads((P/'mutations.json').read_text());check({m['name']for m in mutations}=={'accept-extra-slot','omit-start-roi-binding','ignore-rotation','wrong-literal-route'},'mutation set')
 for m in mutations:check(m['exit_code']!=0 and m['source_sha256']==sources['app/core/gimpfilterlayer.cpp'] and 'ERROR'in(m['stdout']+m['stderr']),'causal mutation')
 ms=json.loads((P/'mutation-seal.json').read_text());check(ms['baseline']==seal and ms['linked_unchanged']and ms['source_unchanged'],'mutation seal')
 runtime=json.loads((P/'runtime-before.json').read_text());rs=json.loads((P/'runtime-seal.json').read_text());check(rs['unchanged']and rs['libraries']==len(runtime)==125,'runtime seal')
 binary=json.loads((P/'binary-after.json').read_text());check(binary=={m:v['binary_sha256']for m,v in results.items()},'binary post seal')
 predecessor=json.loads((P/'normalize-predecessor.json').read_text());check(predecessor['commit']=='3b50714ec7'and all(sources[p]==w for p,w in predecessor['unchanged'].items()),'Normalize predecessor')
 check(json.loads((P/'runtime-after-mutants.json').read_text())=={'libraries':125,'unchanged':True},'runtime mutation seal')
 context=json.loads((P/'context/results.json').read_text());cs=json.loads((P/'context/seal.json').read_text())
 check(cs['reused_provider_receipt_sha256']==sha((P/'results.json').read_bytes()) and cs['reused_provider_source_sha256']==sources['app/core/gimpfilterlayer.cpp'],'context provider lineage')
 check(not cs['changed'] and cs['case_counts']=={'normal':15,'sanitizer':11},'context seal')
 for mode,cases in [('normal',CASES+REG),('sanitizer',CASES)]:
  check(set(context[mode]['runs'])==set(cases+['video_native_saved_context_metadata']),'context cases')
  for case,v in context[mode]['runs'].items():result(v,case)
  check(('-fsanitize=address,undefined'in context[mode]['compile'])==(mode=='sanitizer'),'context compiler')
 with tarfile.open(P/'context/sources.tar.gz')as t:
  check({f.name for f in t if f.isfile()}==set(cs['source']),'context archive set')
  for n,h in cs['source'].items():
   check(sha(t.extractfile(n).read())==h,'context source')
   if args.check_source:check(sha((R/n).read_bytes())==h,n)
 # Real result-admission negative controls, independent of artifact checksums.
 good=results['normal']['runs'][CASES[0]]
 for bad in [dict(good,exit_code=1),dict(good,stdout=good['stdout'].replace(CASES[0],'wrong')),dict(good,stdout=good['stdout'].replace('ok 1 ','not ok 1 ',1))]:
  try:result(bad,CASES[0])
  except ValueError:pass
  else:raise ValueError('invalid result admitted')
 subprocess.run(['python3','-B',str(R/'migration/tests/video-shadow/validate.py')],check=True)
 print(f'PASS Video initial14+10, final test-only successor15+11 (same10 sanitizer units), four mutants, three controls, {len(sources)} archived sources; full task/XCF/UI open')
if __name__=='__main__':main()
