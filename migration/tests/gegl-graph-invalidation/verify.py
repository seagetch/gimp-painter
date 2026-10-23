#!/usr/bin/env python3
"""Check immutable native ROI evidence and optionally its owned/current source."""
import argparse,hashlib,json,pathlib,re,struct,subprocess,tarfile
p=argparse.ArgumentParser();p.add_argument('--check-owned-source',action='store_true');p.add_argument('--check-source',action='store_true');a=p.parse_args()
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest();read=lambda n:json.loads((E/n).read_text())
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
r=read('report.json');assert r['task']=='09.013/gegl-graph-invalidation';assert r['original_criterion']=='変更 ROI と座標を source node・投影へ通知し画面更新と再評価範囲が一致'
assert [r[k]for k in ['model_cases','ui_cases','sanitizer_model_cases','sanitizer_ui_cases']]==[6,3,6,3]
assert r['portrait']==[1080,1920] and r['oncanvas_enabled'] and not r['full_source_duties_done'] and not r['whole_current_application_claim']
checkpoints=read('dependency-checkpoints.json');assert checkpoints==r['tested_dependency_checkpoints'] and len(checkpoints)==12
assert all(c==r['base_checkpoint']=='ce40ac4cffa5b6dba48cd9100303c3c94653a5bc'for c in checkpoints.values())
with tarfile.open(E/'tested-sources.tar.gz')as t:
 assert len(r['source_sha256'])==75
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
  if a.check_owned_source:
   data=subprocess.check_output(['git','show',checkpoints[n]+':'+n],cwd=R)if n in checkpoints else(R/n).read_bytes();assert sha(data)==h,n
 fixture=t.extractfile('app/tests/test-painter-graph-invalidation.cpp').read().decode();ui=t.extractfile('app/tests/test-painter-graph-invalidation-ui.inc').read().decode()
 assert '__real_gegl_node_blit_buffer(n,b,r,level,abyss)'in fixture and 'gegl_buffer_signal_connect'in fixture and 'handler->whole_tile'in fixture
 assert 'g_assert_true(s.trace.computed.empty())'in fixture and 'gegl_rectangle_contains(&expected,&r)'in fixture
 assert 'g_assert_true(s.watched_buffer!=gimp_drawable_get_buffer'in fixture
 assert 'g_assert_true(enabled)'in ui and 'gimp_image_flush(s.image);if(f)filter_settle(f);spin(500)'in ui
seal=read('held-source-seal.json')['rows'];assert len(seal)==8
for row in seal:
 if row.get('baseline_exists')is False:assert row['path']=='app/tools/gimpbucketfillbrushtool.h'and row['native_exists']is False
 else:assert r['source_sha256'][row['path']]==row['sha256'] and row['equals_committed_baseline']
cases=['ordinary-root','ordinary-group','hot-cache','negative-offset','clone','gaussian']
for name,count in [('normal-results',6),('restored-results',6),('asan-model-results',6),('ui-results',3),('asan-ui-results',3)]:
 rows=read(name+'.json');assert len(rows)==1;row=rows[0];assert row['result']=='OK'and row['returncode']==0 and 'env'not in row
 assert len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==count
 for case in cases if count==6 else ['ui/0','ui/1','ui/2']:assert '/graph-invalidation/'+case in row['stdout']
 if count==3:
  assert row['stdout'].count('RAW_CANVAS ')==6
  assert row['stdout'].count('RGB=0,0,255')==3 and row['stdout'].count('RGB=255,0,0')==3
for name in ['normal','restored','ui','asan-model','asan-ui']:assert '1080x1920x24, authenticated accepted, empty authority rejected'in(E/(name+'.log')).read_text()
for mode in range(3):
 for stage in ['before','after']:
  data=(E/f'graph-{mode}-{stage}.png').read_bytes();assert data[:8]==b'\x89PNG\r\n\x1a\n' and struct.unpack('>II',data[16:24])==(1080,1920)
 assert(E/f'graph-{mode}-before.png').read_bytes()!=(E/f'graph-{mode}-after.png').read_bytes()
mutants=read('mutation-results.json');assert {x['name']for x in mutants}=={'drop-stack-origin','skip-hot-tile-flush'}
for m in mutants:
 assert m['expected_red'] and m['returncode']==-6
 x=read('mutations/'+m['name']+'/result.json');assert x['result']=='FAIL'and 'ERROR:'in x['stdout']
assert read('mutations/x-only-grid-equivalent-diagnostic/result.json')['result']=='OK'
b=read('sanitizer-scope.json');assert not b['leak_detection']and not b['whole_current_application_claim']and len(b['relevant_instrumented_units'])==12
for u in b['relevant_instrumented_units']:assert u['sha256']==r['source_sha256'][u['path']]and '-fsanitize=address,undefined'in u['sanitize_flags']
d=read('original-duty-snapshot.json');assert len(d['rows'])==19 and len({x['hunk_id']for x in d['rows']})==19
assert all(x['implementation_state']=='TODO'and '09.013/gegl-graph-invalidation'in x['implementation_tasks']for x in d['rows'])
assert {x['path']for x in d['rows']}=={'app/actions/view-commands.c','app/core/gimpdrawablestack.c','app/core/gimpgrouplayer.c','app/core/gimpimage-crop.c','app/core/gimpimage.c','app/core/gimpmarshal.list','app/core/gimpprojection.c'}
inherited=read('inherited-drawable-duty.json');assert any(x['source_id']=='01.002/000933'and x['source_range']=='-0,0 -> 1,429'and x['whole_hunk_checkpoint_status']=='TODO'for x in inherited)
if a.check_owned_source or a.check_source:
 assert(E/'target.meson').read_text()in(R/'app/tests/meson.build').read_text()
 assert read('inherited-drawable-duty.json')==json.loads((R/'migration/contracts/projection-notify.json').read_text())['duties']
print('PASS: model6/UI3 + native ASan/UBSan6/UI3, two causal mutations, 75 source checkpoints, 19+429-line duties retained')
