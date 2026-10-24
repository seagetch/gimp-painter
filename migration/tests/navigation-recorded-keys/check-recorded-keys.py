#!/usr/bin/env python3
"""Original25.011: every retained recorded key transition, unchanged criterion."""
import csv,hashlib,io,json,math,pathlib,re,struct,subprocess,sys
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def require(value,message):
 if not value:raise ValueError(message)
def passed(stdout):return [re.search(r'(/navigation-events/\S+)',s)[1] for s in stdout.splitlines() if s.startswith('ok ') and '# SKIP' not in s]
def check(root=R,e=E):
 m=json.loads((e/'manifest.json').read_text());a=json.loads((e/'audit.json').read_text())
 require(m['task']==a['task']=='25.011' and m['criterion']==a['criterion']=='記録した全キー遷移の期待角に一致' and a['dependency']=='25.010','original criterion/dependency')
 require(a['baseline']=='c8ff52500a2c07c3c06c8f0dc351bb823904b096','pinned original audit baseline')
 require(m['production_fix'] is False and m['whole_port_complete'] is False and m['old_runtime']=='USER-SKIPPED','scope')
 require(m['actual_ui_oncanvas'] is True and m['historical_detector_settings_unchanged'] is True,'UI scope')
 require(m['sanitizer_policy']==dict(address=True,undefined=True,leaks=False,dependencies_instrumented=False),'sanitizer scope')
 require(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 for p,h in m['source_sha256'].items():require(sha((root/p).read_bytes())==h,'source '+p)
 for p,h in m['artifact_sha256'].items():require(sha((e/p).read_bytes())==h,'artifact '+p)
 for p in m['source_sha256']:
  if p.startswith('app/') and not p.startswith('app/tests/'):require((root/p).read_bytes()==subprocess.check_output(['git','show',a['baseline']+':'+p],cwd=R),'production unchanged '+p)
 includes={'app/tests/'+name for name in re.findall(r'#include "(test-painter-navigation-[^"]+\.inc)"',(root/'app/tests/test-painter-navigation-events.c').read_text())}
 for directory in ['legacy-pan','legacy-navigation-applied','legacy-navigation-space']:
  includes.update(str(p.relative_to(R)) for p in (R/'migration/fixtures'/directory).glob('*.inc'))
 require(includes.issubset(m['source_sha256']),'registered includes bound')
 trace='migration/fixtures/legacy-rotation-operations/gui-captures/rotation.tsv';zoom='migration/fixtures/legacy-zoom/zoom.tsv'
 require({trace,zoom,'app/core/gimpprojection.c'}.issubset(m['source_sha256']),'runtime assets and projection bound before build')
 require(m['source_sha256'][trace]=='bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a','immutable trace')
 require(m['source_sha256'][zoom]=='5fcf9b0f0c66b707a6a57de8e617c49ed9d32dd21750bf1670d2e46ce711a21e','immutable zoom asset')
 records=list(csv.DictReader(io.StringIO((root/trace).read_text()),delimiter='\t'));stationary=[r for r in records if r['scene'].startswith('stationary-')];events=[r for r in stationary if int(r['type'])!=-1];keys=[r for r in records if int(r['type']) in [8,9]]
 require(len(records)==370 and len(stationary)==220 and len(events)==198 and len(keys)==66 and all(r['scene'].startswith('stationary-') for r in keys),'all recorded keys scope')
 baseline=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',a['baseline']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R).decode()),delimiter='\t'));selected={r['work_id']:r for r in baseline if r['wbs_task']=='25.011'}
 require(len(selected)==len(a['rows'])==198 and {r['work_id'] for r in a['rows']}==set(selected),'all198 duties')
 sys.path.insert(0,str(R/'tools'));import assign_legacy_hunks as legacy
 sections=legacy.patch_sections(a['old_base'],a['old_revision']);index={r['child_id']:r for r in legacy.read_tsv(R/'migration/inventory/changed-hunks.tsv')}
 for r in a['rows']:
  original=selected[r['work_id']];require(all(r[k]==original[k] for k in ['source_id','path','source_blob','source_range','source_sha256','action','acceptance']),'original duty identity')
  payload=(e/r['payload']).read_bytes();require(payload==sections[(r['path'],index[r['source_id']]['index'])]['payload'] and sha(payload)==r['source_sha256'],'complete original hunk')
  other={(x['work_id'],x['wbs_task']) for x in baseline if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']};require(other=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']},'other owners preserved')
 review=json.loads((e/'source-duty-review.json').read_text());require(len(review['rows'])==198 and {r['work_id'] for r in review['rows']}==set(selected),'198 source routes')
 for row in review['rows']:
  original=selected[row['work_id']];require(row['source_id']==original['source_id'] and row['path']==original['path'] and row['criterion']==m['criterion'],'review duty identity')
 for p,h in review['current_sources_sha256'].items():require(sha((root/p).read_bytes())==h,'reviewed current source')
 registration=json.loads((e/'registration-source.json').read_text());b=subprocess.check_output(['git','show',registration['baseline']+':'+registration['path']],cwd=R);require(sha(b)==registration['sha256']==sha((root/registration['path']).read_bytes()),'baseline registration');require(all("'"+name+"'" in b.decode() for name in registration['required_entries']),'rotation registered')
 groups={'':4,'reset':6,'original':35,'rotation':2,'zoom':10,'space':3};expected_names=[f'/navigation-events/recorded-keys/tool-{t}-mirror-{h}' for t in range(2) for h in range(2)]
 for kind in ['normal','sanitizer']:
  for group,count in groups.items():
   j=json.loads((e/(kind+('-'+group if group else '')+'.json')).read_text());require(set(j)=={'name','result','returncode','duration','stdout','stderr','source_sha256','cases','test_budget_seconds','xvfb'},'allowlisted receipt')
   require(j['result']=='OK' and j['returncode']==0 and 0<j['duration']<60 and j['test_budget_seconds']==60,'result/budget')
   require(j['source_sha256']==m['source_sha256'] and j['xvfb']==m['xvfb'],'source/display')
   require(j['cases']==count and len(passed(j['stdout']))==count,'case count')
   if group:
    name='normal.json' if group=='reset' else 'normal-'+group+'.json';prior=json.loads((R/'migration/tests/navigation-reset-action'/name).read_text());require(passed(j['stdout'])==passed(prior['stdout']),'unchanged reset/original names');continue
   require(passed(j['stdout'])==expected_names,'four registered tools/flips')
   seen=set();record_by_seq={r['seq']:r for r in events}
   for line in j['stdout'].splitlines():
    if not line.startswith('# recorded key tool='):continue
    f=dict(re.findall(r'([\w-]+)=([^ ]+)',line));key=(int(f['tool']),f['seq']);require(key not in seen and key[0] in [0,1] and f['seq'] in record_by_seq,'unique exact event');seen.add(key);old=record_by_seq[f['seq']]
    require(all(f[k]==old[k] for k in ['scene','step','type']) and f['mirror']==old['mirror'],'trace event identity')
    require(float(f['old'])==float(old['angle']),'literal old angle')
    angle=float(old['angle']);expected=(-angle if int(old['mirror']) else angle)%360.0
    require(math.isfinite(float(f['native'])) and abs(float(f['native'])-expected)<1e-9,'expected native angle convention')
   require(seen=={(t,r['seq']) for t in range(2) for r in events},'all198 events on both tools including66 keys')
   totals=[dict(re.findall(r'([\w-]+)=([^ ]+)',line)) for line in j['stdout'].splitlines() if line.startswith('# recorded key totals ')]
   require(len(totals)==4 and {(r['tool'],r['mirror']) for r in totals}=={(str(t),str(h)) for t in range(2) for h in range(2)} and all(r['scenes']=='11' and r['events']=='99' and r['keys']=='33' and r['motions']=='44' for r in totals),'native trace totals')
 held=json.loads((e/'held-source-isolation.json').read_text());require(held==json.loads((R/'migration/tests/navigation-reset-action/held-source-isolation.json').read_text()) and len(held['rows'])==8,'eight held exclusions')
 build=json.loads((e/'build-provenance.json').read_text());require({r['build'] for r in build['builds']}=={'build','build-asan'},'both builds')
 for r in build['builds']:
  require(r['options']=={'auto_features':'disabled','debug':False,'optimization':'2','b_ndebug':'false','b_sanitize':'none' if r['build']=='build' else 'address,undefined','headless-tests':'disabled','libunwind':False,'painter-http':'enabled'},'build assertions/instrumentation')
 for name in m['screenshots']:
  b=(e/name).read_bytes();require(b[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',b[16:24])==(1080,1920),'portrait screenshot')
 return dict(status='PASS',original_duties=198,normal_cases=60,sanitizer_cases=60,unique_recorded_key_events=66,key_events_per_configuration=132,all_event_executions_per_configuration=396,production_fix=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
