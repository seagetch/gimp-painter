#!/usr/bin/env python3
"""Bounded original26.005 progress proof; never original-task closure."""
import csv,hashlib,io,json,pathlib,re,subprocess,sys,tarfile
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sha=lambda b:hashlib.sha256(b).hexdigest()
def need(value,label):
 if not value:raise AssertionError(label)
def load(p):return json.loads(p.read_text())
def names(row):return set(re.findall(r'^ok \d+ (/\S+)(?!.*# SKIP).*$',row['stdout'],re.M))
def passed(row):return {line.split()[2] for line in row['stdout'].splitlines() if line.startswith('ok ') and '# SKIP' not in line}
def archive(path,seal):
 with tarfile.open(path) as t:
  need(set(t.getnames())==set(seal),'archive paths')
  for p,h in seal.items():need(sha(t.extractfile(p).read())==h,'archive source '+p)
def check(root=R,e=E):
 m=load(e/'manifest.json');a=load(e/'audit.json')
 need(m['task']=='26.005' and m['criterion']=='zoom・pan・回転・反転に追従' and m['dependency']=='26.004','original acceptance')
 need(m['original_task_complete'] is False and m['closed_original_duties']==0 and m['whole_port_complete'] is False,'open obligations')
 need(m['production_fix'] is True and m['old_runtime_executed'] is False and m['independent_old_transform_claim'] is False and m['old_style_parity_claim'] is False,'oracle boundaries')
 need(m['guide_channel_tolerance']==2 and m['classified_border_maximum']==3 and m['unknown_outside_pixels_allowed']==0,'bounded raster policy')
 need(m['new_ui_oncanvas'] is True and m['historical_event_setup_unchanged'] is True,'oncanvas scope')
 need(m['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True),'portrait')
 need(m['sanitizer']==dict(address=True,undefined=True,leaks=False,dependencies_instrumented=False),'sanitizer boundary')
 for p,h in m['owned_source_sha256'].items():need(sha((root/p).read_bytes())==h,'owned source '+p)
 for p,h in m['artifact_sha256'].items():need(sha((e/p).read_bytes())==h,'artifact '+p)
 need(a['baseline']=='20b2204b11df079c306a680b41ac623f3be75c76' and a['status']=='OPEN' and a['closed_original_duties']==0,'audit scope')
 original=list(csv.DictReader(io.StringIO(subprocess.check_output(['git','show',a['baseline']+':migration/inventory/legacy-port-work-items.tsv'],cwd=R,text=True)),delimiter='\t'))
 selected={r['work_id']:r for r in original if r['wbs_task']=='26.005'}
 need(len(a['rows'])==len(selected)==193 and {r['work_id'] for r in a['rows']}==set(selected),'all193 original duties')
 sys.path.insert(0,str(R/'tools'));import assign_legacy_hunks as legacy
 sections=legacy.patch_sections(a['old_base'],a['old_revision']);index={r['child_id']:r for r in legacy.read_tsv(R/'migration/inventory/changed-hunks.tsv')}
 for row in a['rows']:
  need(all(row[k]==v for k,v in selected[row['work_id']].items()),'original assignment unchanged')
  payload=(e/row['payload']).read_bytes();need(payload==sections[(row['path'],index[row['source_id']]['index'])]['payload'] and sha(payload)==row['source_sha256'],'full pinned hunk')
  need({(x['work_id'],x['task']) for x in row['other_whole_hunk_owners']}=={(x['work_id'],x['wbs_task']) for x in original if x['source_id']==row['source_id'] and x['work_id']!=row['work_id']},'other owners')
 need(a['shared_prior_source_mappings']==176,'prior mapping count')
 provider='app/display/gimpcanvasperspectiveguide.cpp';old=subprocess.check_output(['git','show',a['baseline']+':'+provider],cwd=R,text=True);new=(root/provider).read_text()
 need(old.split('void draw (')[0]==new.split('void draw (')[0] and old.split('cairo_region_t *extents (')[1]==new.split('cairo_region_t *extents (')[1],'production changes only draw')
 seal=load(e/'final/source-before-build.json');need(len(seal)==77,'final scoped source count')
 need({'app/tests/test-painter-perspective-damage.inc','app/tests/test-painter-perspective-path-oracle.cpp','migration/fixtures/legacy-perspective-draw/draw-source.inc','app/core/gimpclonelayer.cpp'}.issubset(seal),'required dependencies bound')
 for p in m['owned_source_sha256']:
  if p in seal:need(seal[p]==m['owned_source_sha256'][p],'runtime owned source')
 archive(e/'final/tested-sources.tar.xz',seal)
 held=load(e/'final/held-source-isolation.json')['rows'];need(len(held)==9 and len({x['path'] for x in held})==9,'nine held providers')
 for row in held:
  if row.get('native_exists') is False:need(row['path'] not in seal,'held absent header')
  else:need(seal[row['path']]==row['sha256'],'held source byte identity')
 clone=[r for r in held if r['path']=='app/core/gimpclonelayer.cpp'][0];need(clone['sha256']=='22adedddcaeb6d70f4bf37e94cf92f3dbe4d1b84c7183097d3c24e49d02b081e','held Clone')
 old_names={r['name']:passed(r) for r in load(e/'baseline/normal.json')};need(sum(map(len,old_names.values()))==72,'original72')
 builds=load(e/'final/build-provenance.json')
 for mode in ['normal','sanitizer']:
  actual={};total=0
  for group in ['original','geometry','angle0','angle1','angle2']:
   rows=load(e/f'final/{mode}-{group}.json');need(len(rows)==(3 if group=='original' else 1),'scoped target count')
   for row in rows:
    need(row['result']=='OK' and row['returncode']==0 and 0<row['duration']<60 and row['test_budget_seconds']==60,'native result/budget')
    need(row['source_sha256']==seal and row['xvfb']==m['xvfb'],'native source/display')
    need(row['executable_sha256']==builds[mode]['executables'][row['name'].split(' / ')[-1]]['sha256'],'tested executable')
    need(not re.search(r'^not ok|ERROR: AddressSanitizer|runtime error:|AddressSanitizer:DEADLYSIGNAL',row['stdout']+row['stderr'],re.M),'native diagnostic')
    ns=passed(row);need(len(ns)==row['cases'],'case count');need(not actual.get(row['name'],set())&ns,'duplicate executed cases');actual.setdefault(row['name'],set()).update(ns);total+=len(ns)
    if group=='original':need(ns==old_names[row['name']],'all unchanged original cases')
    elif group=='geometry':
     need(ns=={'/perspective-ui/damage-ink-bounds','/perspective-ui/old-path-pixels'},'two geometry cases')
     ink=re.findall(r'perspective ink zoom=(\S+) aniso=(\d) angle=(\S+) flip=(\d) points=(\d) pixels=(\d+)',row['stdout']);need(len(ink)==576 and all(int(x[-1])>0 for x in ink),'288+288 ink scenes')
     need(len(re.findall(r'perspective old path mismatched=0$',row['stdout'],re.M))==288 and not re.search(r'mismatched=[1-9]',row['stdout']),'288 old path matches')
    else:
     angle=int(group[-1]);need(ns=={f'/perspective-ui/damage-mutations/{angle}'},'angle case')
     events=re.findall(r'perspective damage angle=(\S+) aniso=(\d) flip=(\d) step=(\d) changed=(\d+) residual=(\d+)',row['stdout']);need(len(events)==56 and len({(x[1],x[2],x[3]) for x in events})==56,'all56 transitions')
     need(all(float(x[0])==[0,37,90][angle] and int(x[4])>0 and int(x[5])==0 for x in events),'actual redraw')
     global_rows=re.findall(r'perspective global all=(\d+) above2=(\d+) guide=(\d+) preexisting-border=(\d+) unexplained=(\d+) maximum=(\d+)',row['stdout']);need(len(global_rows)==56,'global audit retained')
     need(all(int(x[2])==int(x[4])==0 and int(x[1])==int(x[3]) and int(x[5])<=3 for x in global_rows),'no discarded unknown pixels')
  need(total==77,'all77 native cases')
  opts=builds[mode]['options'];need(opts['b_ndebug']=='false' and opts['optimization']=='2' and opts['debug'] is False,'assertions/build policy')
  need(opts['b_sanitize']==('none' if mode=='normal' else 'address,undefined'),'ordinary sanitizer')
 for label,count in [('old-path-red',242),('old-path-integer-red',34)]:
  d=e/'diagnostics'/label;red=load(d/'result.json');need(red['returncode']!=0 and f'mismatched={count}' in red['stdout'],'actual old-path red');archive(d/'tested-sources.tar.xz',red['source_sha256'])
 neg=load(e/'negative-missing-damage/result.json');need(neg['returncode']!=0 and 'guide == 0' in neg['stdout']+neg['stderr'] and 'guide=224' in neg['stdout'],'causal missing-damage control');archive(e/'negative-missing-damage/tested-sources.tar.xz',neg['source_sha256'])
 need(load(e/'diagnostics/damage-masked/result.json')['result']=='TIMEOUT','original timeout preserved')
 fixture=root/'migration/fixtures/legacy-perspective-draw';meta=load(fixture/'source.json');pinned=subprocess.check_output(['git','show',meta['revision']+':'+meta['path']],cwd=R);need(pinned==(fixture/'pinned-source.cpp').read_bytes() and sha(pinned)==meta['source_sha256'],'unchanged old full source')
 text=pinned.decode();start=text.index('void \nCanvasPerspectiveGuide::draw');end=text.index('\n\n\ncairo_region_t*',start);need((text[start:end]+'\n').encode()==(fixture/'draw-source.inc').read_bytes(),'unchanged old draw function')
 for name in ['baseline/model-oncanvas.png','final/normal-oncanvas.png']:
  b=(e/name).read_bytes();need(b[:8]==b'\x89PNG\r\n\x1a\n' and int.from_bytes(b[16:20],'big')==1040 and int.from_bytes(b[20:24],'big')==1800,'actual portrait window')
 return dict(status='PASS',normal_cases=77,sanitizer_cases=77,old_path_scenes=288,ink_scenes=288,actual_redraw_transitions=168,original_task_complete=False,closed_original_duties=0)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
