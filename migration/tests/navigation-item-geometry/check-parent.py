#!/usr/bin/env python3
"""Original25.009 common-matrix evidence; no whole-port/runtime parity claim."""
import csv,hashlib,io,itertools,json,pathlib,re,struct,subprocess,sys
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(v,m):
 if not v:raise ValueError(m)
families={'arc':4,'corner':20,'grid':5,'guide':6,'handle':13,'path':6,'rectangle':4,'rectangle-guides':8}
groups=([f'{p}-{i}' for p,n in [('selection',3),('checker',3),('image-edges',3),('signed',6),('grid-precise',3),('chunks',6)] for i in range(n)]+[f'window-{p}-{i}' for p in ['arc','guide','path','rectangle','rectangle-guides'] for i in range(3)]+['geometry-'+p for p in families])
old_groups={'original':35,'rotation':2,'zoom':10,'space':3}
def paths(stdout):return [re.search(r'(/navigation-events/\S+)',x)[1] for x in stdout.splitlines() if x.startswith('ok ') and '# SKIP' not in x]
def combinations(lines,keys,expected):
 actual=[]
 for line in lines:
  fields=dict(re.findall(r'([\w-]+)=([^ ]+)',line))
  actual.append(tuple(fields[k] for k in keys))
 require(len(actual)==len(set(actual)) and set(actual)==set(expected),'exact scene combinations')
def product(*values):return itertools.product(*[[str(x) for x in v] for v in values])
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text());require(m['task']=='25.009' and m['criterion']=='canvas と overlay が同じ角度を使用','original criterion')
 require(m['original_work_rows']==196 and m['whole_port_complete'] is False and m['old_runtime']=='USER-SKIPPED','scope')
 require(m['actual_ui_oncanvas'] is True and m['historical_detector_settings_unchanged'] is True,'UI policy')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 require(m['build_policy']==dict(optimization='2',debug=False,b_ndebug=False,asan=True,ubsan=True,lsan=False,dependency_instrumentation=False),'build')
 build=json.loads((e/'build-provenance.json').read_text());require(len(build['builds'])==2 and {r['build'] for r in build['builds']}=={'build','build-asan'},'two build configurations')
 for row in build['builds']:
  require(row['build'] in ['build','build-asan'],'known build')
  require(row['options']=={'auto_features':'disabled','debug':False,'optimization':'2','b_ndebug':'false','b_sanitize':'none' if row['build']=='build' else 'address,undefined','headless-tests':'disabled','libunwind':False,'painter-http':'enabled'},'actual build options')
  require(re.fullmatch('[0-9a-f]{64}',row['executable_sha256']) and row['executable_bytes']>0,'tested executable identity')
 required_includes={'app/tests/'+name for name in re.findall(r'#include \"(test-painter-navigation-[^\"]+\.inc)\"',(root/'app/tests/test-painter-navigation-events.c').read_text())}
 for directory in ['legacy-pan','legacy-navigation-applied','legacy-navigation-space']:
  required_includes.update(str(p.relative_to(R)) for p in (R/'migration/fixtures'/directory).glob('*.inc'))
 require(required_includes.issubset(m['source_sha256']),'all registered navigation includes bound')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 original=json.loads((e/'audit.json').read_text());full=json.loads((e/'full-hunk-audit.json').read_text())
 require(original['criterion']==m['criterion'] and len(original['rows'])==len(full['rows'])==196,'original196')
 sys.path.insert(0,str(R/'tools'));import assign_legacy_hunks as legacy
 sections=legacy.patch_sections(full['base_revision'],full['source_revision']);index={r['child_id']:r for r in legacy.read_tsv(R/'migration/inventory/changed-hunks.tsv')}
 require(m['audit_baseline']=='a1144398264064609551cf1744657c5ef868566a','audit baseline')
 baseline_rows=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',m['audit_baseline']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R).decode()),delimiter='\t'));by_id={r['work_id']:r for r in baseline_rows}
 ids=set()
 for a,b in zip(original['rows'],full['rows']):
  require(all(a[k]==b[k] for k in ['work_id','source_id','path']),'row identity');require(a['work_id'] not in ids,'duplicate duty');ids.add(a['work_id'])
  original_row=by_id[a['work_id']];require(original_row['wbs_task']=='25.009' and original_row['action']==a['original_action'] and original_row['acceptance']==a['original_acceptance'],'pinned original work row')
  other={(r['work_id'],r['wbs_task']) for r in baseline_rows if r['source_id']==a['source_id'] and r['work_id']!=a['work_id']}
  require(other=={(r['work_id'],r['task']) for r in a['other_whole_hunk_owners']},'other whole-hunk owners preserved')
  sec=sections[(a['path'],index[a['source_id']]['index'])];payload=(e/b['payload']).read_bytes()
  require(payload==sec['payload'] and sha(payload)==a['source_sha256']==b['sha256'],'full hunk including removals')
  require(a['original_acceptance']==m['criterion'] and a['source_range']==b['range']==sec['header'],'original row acceptance/range')
 reviewed=[]
 for name in ['render-source-duty-review.json','primitive-source-duty-review.json','navigation-source-duty-review.json']:
  review=json.loads((e/name).read_text());reviewed+=review['rows']
  for p,h in review['current_sources_sha256'].items():require(sha((root/p).read_bytes())==h,'reviewed source '+p)
 require(len(reviewed)==196 and {r['work_id'] for r in reviewed}==ids,'all196 source routes')
 coverage=json.loads((e/'coverage.json').read_text());require(coverage['task']=='25.009' and coverage['status']=='FINAL_SOURCE_VERIFIED','coverage completion')
 require(len(coverage['rows'])==196 and {r['work_id'] for r in coverage['rows']}==ids,'coverage196')
 review_by_id={r['work_id']:r for r in reviewed}
 for r in coverage['rows']:
  a=review_by_id[r['work_id']];require(r['source_id']==a['source_id'] and r['path']==a['path'] and r['criterion']==m['criterion'],'coverage identity')
  require(r['source_review'] in ['render-source-duty-review.json','primitive-source-duty-review.json','navigation-source-duty-review.json'],'coverage review')
  require(r['receipts'] and all(name in m['artifact_sha256'] and (e/name).is_file() for name in r['receipts']),'coverage receipts')
 for kind in ['normal','sanitizer']:
  for group in groups+list(old_groups):
   r=json.loads((e/(kind+'-'+group+'.json')).read_text());require(r['result']=='OK' and r['returncode']==0 and 0<r['duration']<60 and r['test_budget_seconds']==60,'result/budget '+group)
   require(set(r)=={'name','result','duration','returncode','stdout','stderr','source_sha256','cases','test_budget_seconds','xvfb'},'allowlisted receipt only')
   require(r['source_sha256']==m['source_sha256'] and r['xvfb']==m['xvfb'],'test source/display')
   expected=old_groups.get(group,1);require(r['cases']==expected and len(paths(r['stdout']))==expected,'TAP count '+group)
   if group in old_groups:
    previous=json.loads((R/'migration/tests/navigation-old-space'/('normal-'+group+'.json')).read_text());require(paths(r['stdout'])==paths(previous['stdout']),'unchanged original case names');continue
   require(paths(r['stdout'])==['/navigation-events/item-geometry/'+group],'case name')
   lines=r['stdout'].splitlines()
   if group.startswith('geometry-'):
    family=group[len('geometry-'):];cases=[x for x in lines if x.startswith('# item geometry family='+family+' ')]
    require(len(cases)==12*families[family],'primitive geometry combinations');combinations(cases,['angle','H','V','variant'],product([0,37,90],range(2),range(2),range(families[family])))
   elif group.startswith('window-'):
    family=group[len('window-'):-2];cases=[x for x in lines if x.startswith('# primitive window family='+family+' ')]
    require(len(cases)==4*families[family] and all(x.endswith('residual=0') for x in cases),'actual primitive clear');combinations(cases,['angle','H','V','variant'],product([[0,37,90][int(group[-1])]],range(2),range(2),range(families[family])))
   elif group.startswith('selection-'):
    cases=[x for x in lines if x.startswith('# selection hide ')];require(len(cases)==4 and all(re.search(r'visible=[1-9][0-9]* residual=0$',x) for x in cases),'actual selection clear');combinations(cases,['angle','H','V'],product([[0,37,90][int(group[-1])]],range(2),range(2)))
   elif group.startswith('checker-'):
    cases=[x for x in lines if x.startswith('# checker ')];require(len(cases)==4 and all(x.endswith('pixels=220900') for x in cases),'checker pixels');combinations(cases,['angle','H','V'],product([[0,37,90][int(group[-1])]],range(2),range(2)))
   elif group.startswith('image-edges-'):
    cases=[x for x in lines if x.startswith('# image edges ')];require(len(cases)==12 and all(all(int(n)>500 for n in re.search(r'inside=(\d+) outside=(\d+)$',x).groups()) for x in cases),'image edge pixels');combinations(cases,['angle','zoom','H','V'],product([[0,37,90][int(group[-1])]],['0.5','1','4'],range(2),range(2)))
   elif group.startswith('signed-'):
    require(any(x.startswith('# signed geometry ') and x.endswith('scenes='+('384' if int(group[-1])>=3 else '32')) for x in lines),'signed byte-equivalence scenes')
   elif group.startswith('grid-precise-'):
    cases=[x for x in lines if x.startswith('# precise grid ')];require(len(cases)==40 and all(re.search(r'visible=[1-9][0-9]* residual=0$',x) for x in cases),'precise grid clear');combinations(cases,['angle','H','V','style','type'],product([[0,37,90][int(group[-1])]],range(2),range(2),range(2),range(5)))
   elif group.startswith('chunks-'):
    cases=[x for x in lines if x.startswith('# chunk interiors ')];require(len(cases)==4 and all(int(re.search(r'pixels=(\d+)',x)[1])>20000 for x in cases),'chunk pixels');combinations(cases,['angle','H','V','aniso'],product([[0,37,90][int(group[-1])%3]],range(2),range(2),[int(group[-1])//3]))
 held=json.loads((e/'held-source-isolation.json').read_text());require({r['path'] for r in held['rows']}=={'app/widgets/gimpcellrendererpopup.cpp','app/display/gimppaintercanvasui.cpp','app/core/gimppaintermybrush.cpp','app/core/gimpviewable.c','app/widgets/gimpviewrenderer.c','app/tools/gimpfillbrushtool.cpp','app/tools/gimpbucketfillbrushtool.h','app/tools/gimp-tools.c'},'held source list')
 for r in held['rows']:
  require(r['baseline']=='ce40ac4cffa5b6dba48cd9100303c3c94653a5bc','held baseline')
  old=subprocess.run(['git','show',r['baseline']+':'+r['path']],cwd=R,capture_output=True)
  if old.returncode:require(r.get('baseline_exists') is False and r.get('native_exists') is False,'held absent header')
  else:require(r.get('equals_committed_baseline') is True and sha(old.stdout)==r['sha256'],'held source isolation')
 for name in m['screenshots']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'actual portrait screenshot')
 return dict(status='PASS',original_duties=196,new_cases=47,original_cases=50,normal_cases=97,sanitizer_cases=97,whole_port_complete=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
