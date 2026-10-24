#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,re,csv,difflib
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def body(s,needle):
 a=s.index(needle);b=s.index('{',a);depth=1;i=b+1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
def ledger_admit(historical,current):
 import importlib.util
 p=R/'migration/tests/smudge-accumulator-closure/ledger-admission.py'
 assert sha(p.read_bytes())=='9924df43db71bfa891d2181bdb427a175e239e610e1df63c1429d38ca1e0b24e'
 spec=importlib.util.spec_from_file_location('smudge_ledger_admission',p);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
 m.admit(historical,current)
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 a=json.loads((p/'audit.json').read_text());assert a['tasks']==['23.002']and a['status']=='OPEN'and a['closed_original_duties']==0 and len(a['rows'])==52
 current={r['work_id']:r for r in csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t')}
 for row in a['rows']:
  ledger_admit(row,current[row['work_id']])
  assert {(x['work_id'],x['wbs_task'])for x in current.values()if x['source_id']==row['source_id']and x['work_id']!=row['work_id']}=={(x['work_id'],x['task'])for x in row['other_whole_hunk_owners']}
 review=json.loads((p/'source-duty-review.json').read_text());assert len(review['rows'])==52 and {r['work_id']for r in review['rows']}==set(r['work_id']for r in a['rows']);assert all(r['status']=='OPEN_REVIEW_NOT_CLOSURE'and r['original_criterion']=='初期採色と更新が一致'for r in review['rows'])
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==26
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz')as t:
  assert set(t.getnames())==set(seal);src={n:t.extractfile(n).read()for n in seal}
  for n,h in seal.items():assert sha(src[n])==h,n
 assert sha((R/'migration/tests/run_dynamics_current_sanitizers.py').read_bytes())==seal['migration/tests/run_dynamics_current_sanitizers.py']
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==10
 for row in held:
  if row.get('native_exists')is False:assert row['path']not in seal
  else:assert seal[row['path']]==row['sha256']
 proof=json.loads((p/'committed-source-proof.json').read_text());eq=json.loads((p/'final/current-equivalence.json').read_text());assert proof['commit']==eq['main_commit']and len(proof['rows'])==8
 main={}
 for row in proof['rows']:
  b=(p/row['artifact']).read_bytes();assert sha(b)==row['sha256'];assert hashlib.sha1(b'blob '+str(len(b)).encode()+b'\0'+b).hexdigest()==row['git_blob'];main[row['path']]=b
 assert set(eq['whole_equal'])=={'app/core/gimpdynamics.c','app/core/gimpdynamicsoutput.c','app/core/gimpcurve.c','app/tests/test-painter-dynamics.c','app/tests/test-painter-brush-geometry.cpp'}
 for n in eq['whole_equal']:assert src[n]==main[n]
 assert len(eq['functions'])==2
 for row in eq['functions']:
  n=row['path'];a=body(main[n].decode(),row['function']);b=body(src[n].decode(),row['function']);assert a==b==row['body']and sha(a.encode())==row['body_sha256'];assert sha(main[n])==row['main_sha256']and seal[n]==row['private_sha256']
 n='app/core/core-enums.c';diff=list(difflib.unified_diff(src[n].decode().splitlines(),main[n].decode().splitlines()));changes=[x for x in diff if x[:1]in ['+','-']and not x.startswith(('+++','---'))];assert len(changes)==4 and all(x.startswith('+')and('GIMP_UNDO_GROUP_MYPAINT'in x or'GIMP_UNDO_MYPAINT'in x)for x in changes)
 def enum(b):s=b.decode();return s[s.index('GIMP_DYNAMICS_OUTPUT_OPACITY'):s.index('} GimpDynamicsOutputType;')]
 assert enum(src['app/core/core-enums.h'])==enum(main['app/core/core-enums.h'])
 names=['defaults-enum','independent-flow','roundtrip-copy','legacy-key','empty-curve']
 def five(out):assert re.findall(r'^ok \d+ /painter/dynamics/(\S+)',out,re.M)==names and 'not ok'not in out and '# SKIP'not in out
 def neutral(out):assert 'ok 1 /painter/geometry/default-dynamics'in out and 'not ok'not in out and '# SKIP'not in out
 for filename,fn in [('normal-dynamics.json',five),('normal-neutral.json',neutral)]:
  row=json.loads((p/'final'/filename).read_text());assert row['result']=='OK'and row['returncode']==0 and row['duration']<60;fn(row['stdout']);assert re.fullmatch('[a-f0-9]{64}',row['executable_sha256'])
 assert '1080x1920x24, authenticated accepted, empty authority rejected'in(p/'final/normal-neutral.log').read_text()
 driver=src['app/tests/test-painter-brush-geometry.cpp'].decode();assert 'i<1028'in driver and 'count,==,256'in driver and 'painter-legacy-brush-geometry'in driver
 base=json.loads((p/'final/five-c-units.json').read_text());assert base['exit_code']==0 and not base['changed_during_build'];five(base['stdout']);assert len(base['sources_sha256'])==5 and all(n.endswith('.c')for n in base['sources_sha256'])
 for n,h in base['sources_sha256'].items():assert seal[n]==h
 assert base['sanitizers']==['address','undefined']and not base['leak_detection'];assert all('-fno-sanitize'not in ' '.join(c)for c in base['commands'])
 san=json.loads((p/'final/sanitizers.json').read_text());assert san['five_c_report_sha256']==sha((p/'final/five-c-units.json').read_bytes())and san['exit_code']==0 and not san['changed_during_run'];neutral(san['stdout'])
 for n,h in san['sources_sha256'].items():assert seal[n]==h
 assert '-fsanitize=address,undefined'in san['link_command']and any(x.endswith('/dynamics-sanitizers/app_core_libappcore.a')for x in san['link_command']);assert not any('-fno-sanitize'in x for x in san['link_command'])
 q=p/'final/neutral-causal-control';red=json.loads((q/'result.json').read_text());assert red['result']=='FAIL'and red['returncode']!=0 and 'original_default_dynamics'in red['stdout']+red['stderr']
 restored=json.loads((q/'restored-result.json').read_text());assert restored['result']=='OK'and restored['returncode']==0;neutral(restored['stdout'])
 pins=json.loads((q/'source-before-build.json').read_text());n='app/core/gimpdynamicsoutput.c';assert set(pins)==set(seal)and {k for k in seal if seal[k]!=pins[k]}=={n};mut=(q/'mutant.c').read_bytes();assert mut==src[n].replace(b'if (! options || ! gimp_painter_brush_geometry_options (options) ||',b'if (TRUE || ! options || ! gimp_painter_brush_geometry_options (options) ||')and sha(mut)==pins[n]
 print('PASS: current5 dynamics +1028-value registered neutral curve normal/5C sanitizers;causal mapping bypass rejected;exact committed role equivalence;10 held;52 duties OPEN')
if __name__=='__main__':check()
