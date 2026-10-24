#!/usr/bin/env python3
from pathlib import Path
import argparse,hashlib,json,re,tarfile,subprocess
P=Path(__file__).resolve().parent;R=P.parents[2]
BASE='0a775df4135052a2c851c9a5dfa37cea25887268'
N=['normalize_gray_noalpha_owner','normalize_gray_cancel','normalize_gray_routes','normalize_gray_context','normalize_gray_undefined_recovery','filter_normalize_native_basic','normalize_unsupported_formats','normalize_late_context_routes','normalize_component_masks','normalize_cancel_and_resume','normalize_outside_selection_no_merge','normalize_rgb_rgba_inputs','normalize_noalpha_owner_rejected','normalize_projection_extent_cache','normalize_invalid_arguments','normalize_undefined_preserves_cache_history']
NR=['filter_red_eye_native_basic','filter_colorify_native_basic','filter_color_exchange_native_basic']
X=['normalize_gray_native_roundtrips','normalize_old_wire_admission','normalize_wire_roundtrips','normalize_malformed_wire','normalize_native_model_precedence','normalize_undefined_cache_roundtrip']
XR=['red_eye_old_wire_admission','red_eye_old_wire_roundtrips','red_eye_malformed_wire','exchange_old_wire_admission','exchange_wire_roundtrips','exchange_malformed_wire','exchange_native_model_precedence','colorify_typed_xcf_roundtrips','colorify_unsupported_typed_xcf_roundtrips']
sha=lambda b:hashlib.sha256(b).hexdigest()
def check(ok,why):
 if not ok:raise ValueError(why)
def read(p):return json.loads(p.read_text())
def admit(r,case,prefix):check(r['exit_code']==0 and re.findall(r'^ok \d+ (\S+)',r['stdout'],re.M)==['/'+prefix+'/'+case],'TAP result')
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--check-source',action='store_true');ap.add_argument('--check-index',action='store_true');a=ap.parse_args()
 files=read(P/'artifact-sha256.json');check(set(files)=={str(f.relative_to(P))for f in P.rglob('*')if f.is_file()and f.name!='artifact-sha256.json'and '__pycache__'not in f.parts},'artifact set')
 for p,h in files.items():check(sha((P/p).read_bytes())==h,p)
 scope=read(P/'scope.json');check(scope['base_commit']==BASE and scope['only_normalize_branch_changed'],'scope');baseline=read(P/'baseline-source-sha256.json');archives={}
 for name,cases,reg,prefix,units in [('native',N,NR,'gimp-filter-layer',10),('xcf',X,XR,'painter-xcf-open',11)]:
  D=P/name;seal=read(D/'seal.json');owned=read(D/'owned.json');sources=read(D/'source-sha256.json');results=read(D/'results.json');check(seal['base_commit']==BASE and seal['owned']==owned and not seal['changed'],'seal')
  check(sha((P/'baseline-source-sha256.json').read_bytes())==seal['manifest_sha256'] and len(baseline)==seal['base_sources'],'baseline')
  check(len(read(D/'linked-before.json'))==seal['linked_inputs'],'actual link inputs')
  data={}
  with tarfile.open(D/'sources.tar.gz')as t:
   for f in t:
    check(f.isfile()and not Path(f.name).is_absolute()and '..'not in Path(f.name).parts and f.name not in data,'archive path');data[f.name]=t.extractfile(f).read()
  check({p:sha(b)for p,b in data.items()}==sources,'archive source hashes');archives[name]=data
  for p,h in owned.items():check(sources[p]==h,'owned archived')
  for mode,wanted in [('normal',cases+reg),('sanitizer',cases)]:
   check(set(results[mode]['runs'])==set(wanted),'case set')
   for case,row in results[mode]['runs'].items():admit(row,case,prefix)
   for p,h in results[mode]['sources'].items():check(sources[p]==h,'instrumented exact source')
  cmds=[c for c in results['sanitizer']['commands']if '-c'in c];check(len(cmds)==units and len(results['sanitizer']['sources'])==units,'selected units')
  check(all('-fsanitize=address,undefined'in c and '-fno-sanitize=vptr'in c for c in cmds),'sanitizer flags')
  cpp=[c for c in cmds if any(v.endswith('.cpp')for v in c)];check(len(cpp)==(9 if name=='native'else 4),'C++ count');check(sum('-fno-rtti'in c for c in cpp)==(7 if name=='native'else 4),'RTTI scope')
  if a.check_source:
   for p,h in owned.items():
    if p!='app/tests/test-painter-xcf-open.c':check(sha((R/p).read_bytes())==h,p)
 old=subprocess.check_output(['git','show',BASE+':app/core/gimpfilterlayer.cpp'],cwd=R).decode();core=archives['native']['app/core/gimpfilterlayer.cpp'].decode();check(sha(old.encode())==scope['core_baseline_sha256'] and sha(core.encode())==scope['core_current_sha256'],'core pins')
 before='gimp_drawable_get_base_type (GIMP_DRAWABLE (owner)) == GIMP_RGB &&';after='(gimp_drawable_get_base_type (GIMP_DRAWABLE (owner)) == GIMP_RGB ||\n             gimp_drawable_get_base_type (GIMP_DRAWABLE (owner)) == GIMP_GRAY) &&'
 start=old.index('    else if (procedure == "plug-in-normalize")');end=old.index('    else if (procedure == "plug-in-red-eye-removal")',start)
 branch=old[start:end].replace('only nonlinear U8 RGB\n','only nonlinear U8 RGB/Gray\n').replace('are no user arguments. Gray/indexed native ownership remains open.','are no user arguments. Indexed palette ownership remains separate.').replace(before,after)
 check(core==old[:start]+branch+old[end:],'only exact Gray admission change');check(archives['xcf']['app/core/gimpfilterlayer.cpp']==core.encode(),'XCF same tested core')
 aux=read(P/'xcf/native-predecessor.json');check(aux['seal']['base_commit']==BASE and not aux['seal']['changed'],'XCF native predecessor');
 for p,h in aux['auxiliary_source_sha256'].items():check(h==(scope['core_current_sha256']if p=='app/core/gimpfilterlayer.cpp'else baseline[p]),'XCF exact linked core/raw predecessor')
 v=read(P/'xcf-shared-source.json');check(sha(archives['xcf'][v['path']])==v['virtual_tracked_sha256'] and v['tested_equal'],'virtual fixture')
 if a.check_index:check(sha(subprocess.check_output(['git','show',':'+v['path']],cwd=R))==v['virtual_tracked_sha256'],'staged fixture differs tested')
 rs=read(P/'expected-red/seal.json');check(rs['base_commit']==BASE and not rs['changed'] and rs['baseline_sources']==len(baseline),'red source seal');check(len(read(P/'expected-red/linked-before.json'))==rs['linked_inputs'],'red actual link count')
 red=read(P/'expected-red/results.json')['gray'];check(red['exit_code']!=0 and 'Saved filter procedure or argument mapping is unsupported'in red['stdout']+red['stderr'],'causal Gray red')
 ms=read(P/'native/mutations.json');check({m['name']for m in ms}=={'deny-gray','omit-gray-roi','accept-extra-slot'},'mutant set')
 for m in ms:check(m['exit_code']!=0 and m['source_sha256']==scope['core_current_sha256'] and 'ERROR'in m['stdout']+m['stderr'],'mutant oracle')
 check(read(P/'native/mutation-seal.json')['baseline']==read(P/'native/seal.json'),'mutation checkpoint')
 oracle=read(P/'oracle/results.json');check(oracle['kernel_sha256']==sha(archives['native']['app/painter/filter-normalize-kernel.hpp']),'kernel pin')
 for name in ['test-oracle.c','old-normalize.inc']:check((P/'oracle'/name).read_bytes()==(R/'migration/tests/normalize-foundation'/name).read_bytes(),'literal old host')
 check({x['name']for x in oracle['runs']}=={'normal','sanitizer','scale','flat','alpha'},'scalar set')
 for x in oracle['runs']:
  if x['name']in ['normal','sanitizer']:check(x['code']==0 and 'PASS 65792'in x['stdout'],'scalar success')
  else:check(x['code']!=0 and 'Assertion'in x['stderr'],'scalar mutant')
 good=read(P/'native/results.json')['normal']['runs'][N[0]]
 for bad in [dict(good,exit_code=1),dict(good,stdout=good['stdout'].replace(N[0],'wrong')),dict(good,stdout=good['stdout'].replace('ok 1 ','not ok 1 ',1))]:
  try:admit(bad,N[0],'gimp-filter-layer')
  except ValueError:pass
  else:raise ValueError('bad result admitted')
 for n,h in scope['predecessor_manifests'].items():
  check(sha((R/'migration/tests'/n/'artifact-sha256.json').read_bytes())==h,'predecessor pin');subprocess.run(['python3','-B',str(R/'migration/tests'/n/'validate.py')],check=True)
 print('PASS bounded Gray Normalize:65792 scalar normal/san,19 native/16 selected10-unit ASan,15 XCF/6 selected11-unit ASan,4 Gray SaveOpen cycles,6 causal mutants,3 result controls')
if __name__=='__main__':main()
