#!/usr/bin/env python3
"""Validate exact bounded Normalize XCF private-native receipts, no application run."""
from pathlib import Path
import argparse,hashlib,json,re,tarfile,subprocess,struct
P=Path(__file__).resolve().parent;R=P.parents[2]
CASES=['normalize_old_wire_admission','normalize_wire_roundtrips','normalize_malformed_wire','normalize_native_model_precedence','normalize_undefined_cache_roundtrip']
REG=['red_eye_old_wire_admission','red_eye_old_wire_roundtrips','red_eye_malformed_wire','exchange_old_wire_admission','exchange_wire_roundtrips','exchange_malformed_wire','exchange_native_model_precedence','colorify_typed_xcf_roundtrips','colorify_unsupported_typed_xcf_roundtrips']
def sha(b):return hashlib.sha256(b).hexdigest()
def check(b,m):
 if not b:raise ValueError(m)
def result(r,case):
 check(r['exit_code']==0,'failed result')
 check(re.findall(r'^ok \d+ (\S+)',r['stdout'],re.M)==['/painter-xcf-open/'+case],'wrong TAP case')
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
 check(len(results['sanitizer']['sources'])==11 and scope['instrumented_units']==11 and scope['cpp_units_no_rtti']==4 and scope['c_units']==7 and not scope['vptr_sanitizer'] and not scope['leak_detection'],'sanitizer scope')
 commands=[c for c in results['sanitizer']['commands']if '-c'in c]
 check(len(commands)==11 and all('-fsanitize=address,undefined'in c and '-fno-sanitize=vptr'in c for c in commands),'actual sanitizer compiler flags')
 cpp=[c for c in commands if any(x.endswith('.cpp')for x in c)]
 check(len(cpp)==4 and all('-fno-rtti'in c for c in cpp),'actual RTTI compiler scope')
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
  for n in list(owned):check(sha((R/n).read_bytes())==sources[n],n)
 core=data['app/xcf/painter-xcf-load.cpp'].decode();a=core.index('  if (name && std::strcmp (name, "plug-in-normalize")');z=core.index('  if (name && std::strcmp (name, "plug-in-red-eye-removal")',a);old=core[:a]+core[z:];check(sha(old.encode())==scope['loader_before_sha256'] and scope['only_new_normalize_branch'],'unrelated loader drift')
 native=json.loads((P/'native-predecessor.json').read_text());aux=json.loads((P/'auxiliary-fixture.json').read_text());check(sources[aux['path']]==aux['sha256'] and aux['commit']=='f5398e02c4b16aa7e1f91150a75113bdbae5b1e0','committed Colorify fixture')
 check(sha((R/'migration/tests/normalize-native/artifact-sha256.json').read_bytes())==scope['normalize_native_artifacts_sha256'],'native predecessor pin')
 for n,h in native['auxiliary_source_sha256'].items():check(sources[n]==h and native['seal']['owned'][n]==h,'native source predecessor')

 red=json.loads((P/'expected-red.json').read_text());check(red['exit_code']!=0 and "'args' should not be NULL"in red['stdout'],'missing-route red')
 mutations=json.loads((P/'mutations.json').read_text());check({m['name']for m in mutations}=={'lose-run-bits','wrong-context-type','accept-wrong-declared-size'},'mutation set')
 for m in mutations:check(m['exit_code']!=0 and m['source_sha256']==sources['app/xcf/painter-xcf-load.cpp'] and 'ERROR'in(m['stdout']+m['stderr']),'causal mutation')
 ms=json.loads((P/'mutation-seal.json').read_text());check(ms['baseline']==seal and ms['linked_unchanged']and ms['source_unchanged'],'mutation seal')
 # Independently recheck all literal old-writer bytes, including signed limits.
 writer=json.loads((P/'writer.json').read_text());code=(P/'writer-oracle.c').read_text();meta=json.loads((P/'writer-source.json').read_text());check(sha(code.encode())==writer['source_sha256'],'writer source')
 for n,b in meta['exact_functions'].items():check(b in code,n)
 def wire_ok(stdout):
  values=[]
  for line in stdout.splitlines():
   tag,r,h=line.split();r=int(r);name=b'plug-in-normalize\0';expected=struct.pack('>I',len(name))+name+struct.pack('>III',2,4,r&0xffffffff)+struct.pack('>IIII',1,0,10,0)+b'\0'*8
   check(tag=='WIRE'and len(expected)==58 and bytes.fromhex(h)==expected,'writer bytes');values.append(r)
  check(values==[-2147483648,-123,-1,0,1,2,123,2147483647],'writer run domain')
 check(set(writer['runs'])=={'normal','sanitizer'},'writer modes')
 for r in writer['runs'].values():check(r['exit_code']==0 and not r['stderr'],'writer runtime');wire_ok(r['stdout'])
 check({m['name']for m in writer['mutants']}=={'wrong-run-tag','wrong-drawable-tag'},'writer mutants')
 for m in writer['mutants']:
  try:wire_ok(m['stdout'])
  except ValueError:pass
  else:raise ValueError('writer mutation admitted')
 # Real result-admission negative controls, independent of artifact checksums.
 good=results['normal']['runs'][CASES[0]]
 for bad in [dict(good,exit_code=1),dict(good,stdout=good['stdout'].replace(CASES[0],'wrong')),dict(good,stdout=good['stdout'].replace('ok 1 ','not ok 1 ',1))]:
  try:result(bad,CASES[0])
  except ValueError:pass
  else:raise ValueError('invalid result admitted')
 subprocess.run(['python3','-B',str(R/'migration/tests/normalize-native/validate.py')],check=True)
 print(f'PASS Normalize XCF native14, selected11-unit sanitizer5,36 Save/Open cycles,three decoder/two writer mutants,three controls,{len(sources)} archived sources; full task/UI open')
if __name__=='__main__':main()
