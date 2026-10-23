#!/usr/bin/env python3
"""Original25.010 registered reset action/state/UI evidence, not whole port."""
import csv,hashlib,importlib.util,io,itertools,json,pathlib,re,struct,subprocess
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(value,message):
 if not value:raise ValueError(message)
def passed(stdout):return [re.search(r'(/navigation-events/\S+)',line)[1] for line in stdout.splitlines() if line.startswith('ok ') and '# SKIP' not in line]
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text());a=json.loads((e/'audit.json').read_text())
 require(m['task']==a['task']=='25.010' and m['criterion']==a['criterion']=='状態と UI 表示が一致' and a['dependency']=='25.009','original criterion/dependency')
 require(m['production_fix'] is False and m['whole_port_complete'] is False and m['old_runtime']=='USER-SKIPPED','scope')
 require(m['sanitizer_policy']==dict(address=True,undefined=True,leaks=False,dependencies_instrumented=False),'sanitizer boundary')
 require(m['actual_ui_oncanvas'] is True and m['historical_detector_settings_unchanged'] is True,'UI policy')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p in m['source_sha256']:
  if p.startswith('app/') and not p.startswith('app/tests/'):
   require((root/p).read_bytes()==subprocess.check_output(['git','show',a['baseline']+':'+p],cwd=R),'production unchanged '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 includes={'app/tests/'+name for name in re.findall(r'#include "(test-painter-navigation-[^"]+\.inc)"',(root/'app/tests/test-painter-navigation-events.c').read_text())}
 require(includes.issubset(m['source_sha256']),'all navigation includes bound')
 require({'app/actions/view-actions.c','app/actions/view-commands.c','app/display/gimpstatusbar.c','app/widgets/gimpuimanager.c','app/widgets/gimpaction.c','app/widgets/gimpenumaction.c'}.issubset(m['source_sha256']),'actual action/UI source bound')
 fixture=(root/'app/tests/test-painter-navigation-reset-action.inc').read_text()
 body=fixture[fixture.index('reset_action_activate (void)'):fixture.index('reset_action_assert_state (')]
 require('gimp_ui_manager_activate_action (ui, "view", "view-rotate-reset")' in body and 'gimp_display_shell_rotate_to' not in body,'registered action not helper substitute')
 require('0f71f055a5fc3bb2237a56303738a41d5d503663'==a['baseline'],'baseline')
 baseline=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',a['baseline']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R).decode()),delimiter='\t'))
 selected=[r for r in baseline if r['wbs_task']=='25.010'];require(len(selected)==len(a['rows'])==2,'two original duties')
 for r in a['rows']:
  old=next(x for x in selected if x['work_id']==r['work_id'])
  require(all(r[k]==old[k] for k in ['source_id','path','source_blob','source_range','source_sha256','action','acceptance']),'original duty')
  require(r['acceptance']==m['criterion'],'duty criterion')
  require(sha((e/r['full_payload']).read_bytes())==r['source_sha256'],'full pinned hunk')
  source=subprocess.check_output(['git','show',a['old_revision']+':'+r['path']],cwd=R);require((e/'pinned-source'/r['path']).read_bytes()==source,'old full source')
  other={(x['work_id'],x['wbs_task']) for x in baseline if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']}
  require(other=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']},'other owners')
 expected=[f'/navigation-events/reset-action/state-{i}' for i in range(4)]+['/navigation-events/reset-action/reentry-default','/navigation-events/reset-action/reentry-custom']
 old_groups={'original':35,'rotation':2,'zoom':10,'space':3}
 for kind in ['normal','sanitizer']:
  for group,count in [('',6)]+list(old_groups.items()):
   name=kind+('-'+group if group else '')+'.json';j=json.loads((e/name).read_text())
   require(set(j)=={'name','result','returncode','duration','stdout','stderr','source_sha256','cases','test_budget_seconds','xvfb'},'allowlisted receipt')
   require(j['result']=='OK' and j['returncode']==0 and 0<j['duration']<60 and j['test_budget_seconds']==60,'native result/budget')
   require(j['source_sha256']==m['source_sha256'] and j['xvfb']==m['xvfb'],'source/display')
   require(j['cases']==count and len(passed(j['stdout']))==count,'case count')
   if group:
    old=json.loads((R/'migration/tests/navigation-item-geometry'/('normal-'+group+'.json')).read_text());require(passed(j['stdout'])==passed(old['stdout']),'original50 case names');continue
   require(passed(j['stdout'])==expected,'reset registered names')
   state=[dict(re.findall(r'([\w-]+)=([^ ]+)',line)) for line in j['stdout'].splitlines() if line.startswith('# reset action ')]
   want=set(itertools.product(['0','1'],['0','1'],['0','1'],['0','37','90','180','359.999','-37']))
   got=[tuple(r[k] for k in ['H','V','entry','angle']) for r in state]
   require(len(got)==48 and set(got)==want and all(r['after']=='0' and r['repeat-residual']=='0' and r['zoom']==('1.25' if r['entry']=='1' else '1') for r in state),'48 state/GUI/repeat scenes')
   entry=[dict(re.findall(r'([\w-]+)=([^ ]+)',line)) for line in j['stdout'].splitlines() if line.startswith('# reset reentry ')]
   require(len(entry)==8 and {tuple(r[k] for k in ['custom','H','V']) for r in entry}==set(itertools.product(['0','1'],repeat=3)) and all(r['reset']=='0' and float(r['moved-angle'])!=0 for r in entry),'8 immediate default/custom reentries')
 spec=importlib.util.spec_from_file_location('reset_runtime_dependencies',E/'check-runtime-dependencies.py');dependency=importlib.util.module_from_spec(spec);spec.loader.exec_module(dependency);require(dependency.check(root,e)['native_hash_before_parse_sites']==3,'retrospective native fixture dependencies')
 held=json.loads((e/'held-source-isolation.json').read_text());require(len(held['rows'])==8 and held==json.loads((R/'migration/tests/navigation-item-geometry/held-source-isolation.json').read_text()),'eight held-source baseline paths')
 build=json.loads((e/'build-provenance.json').read_text());require({r['build'] for r in build['builds']}=={'build','build-asan'},'two native configurations')
 for row in build['builds']:
  require(row['options']=={'auto_features':'disabled','debug':False,'optimization':'2','b_ndebug':'false','b_sanitize':'none' if row['build']=='build' else 'address,undefined','headless-tests':'disabled','libunwind':False,'painter-http':'enabled'},'assertions/sanitizers/flags')
 for name in m['screenshots']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'portrait capture')
 return dict(status='PASS',original_duties=2,reset_cases=6,normal_cases=56,sanitizer_cases=56,ui_scenes_per_configuration=56,action_activations_per_configuration=112,production_fix=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
