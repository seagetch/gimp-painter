#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--check-owned-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='09.011/native-stroke-undo' and r['native_tap']==16 and r['sanitizer_tap']==16 and not r['publishes_history_in_factory'] and not r['real_engine_pairing_complete'] and not r['rendered_ui_evidence'] and r['oncanvas_enabled'] and not r['whole_application_instrumented']
dependencies=json.loads((E/'dependency-checkpoints.json').read_text());assert set(dependencies)=={'app/core/gimpimage-undo.c'}
seal={x['path']:x for x in json.loads((E/'held-source-seal.json').read_text())};assert len(seal)==8
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
  if n in seal:assert seal[n]['tested']==h,n
  if a.check_owned_source:
   if n in seal:assert sha(subprocess.check_output(['git','show',seal[n]['checkpoint']+':'+n],cwd=R))==h,n
   elif n in dependencies:assert sha(subprocess.check_output(['git','show',dependencies[n]+':'+n],cwd=R))==h,n
   else:assert sha((R/n).read_bytes())==h,n
 production=t.extractfile('app/paint/gimpmypaintcoreundo.cpp').read().decode();assert 'state->apply(mode);\n GIMP_UNDO_CLASS(gimp_mypaint_core_undo_parent_class)->pop' in production
 assert 'gimp_image_undo_push' not in production and 'gpointer stroke' not in production
 assert 'mutable std::shared_ptr<StrokeState> prepared_lease' in production and 'if(u->preparing||u->guarded)' in production
 fixture=t.extractfile('app/tests/test-painter-stroke-undo.cpp').read().decode();assert '"painter-canvas-ui",TRUE' in fixture and 'g_assert_true(enabled)'in fixture
rows=[json.loads(s)for s in(E/'native.json').read_text().splitlines()];assert len(rows)==1 and rows[0]['result']=='OK' and 'env'not in rows[0]
for s in [rows[0]['stdout'],(E/'asan-portrait.log').read_text(),(E/'restored-portrait.log').read_text()]:
 assert len(re.findall(r'^ok \d+ ',s,re.M))==16
 for n in ['observer-coherence','update-dispose','dirty-dispose','prepare-dispose','finish-reentry','guard-once','factory-reentry','buffer-observer-coherence','buffer-observer-dispose','nested-buffer-freeze']:assert '/stroke-undo/'+n in s
for n in ['normal-portrait.log','asan-portrait.log','restored-portrait.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected'in(E/n).read_text()
rebuild=json.loads((E/'held-native-rebuild.json').read_text());assert len(rebuild['rebuild_lines'])==3 and len(rebuild['objects'])==2
assert any('gimp-tools.c.o'in x for x in rebuild['rebuild_lines']) and any('gimpfillbrushtool.cpp.o'in x for x in rebuild['rebuild_lines'])
b=json.loads((E/'asan-build.json').read_text());assert len(b['instrumented_sources'])==9 and not b['changed_during_build'] and b['mutant']is None and not b['vptr_sanitizer'] and not b['leak_detection']
for n,h in b['sources_sha256'].items():assert r['source_sha256'][n]==h,n
assert {m['name']for m in r['mutants']}=={'state-after-pixels','premature-free','skip-state-apply','unbatched-payload'}
for m in r['mutants']:assert m['exit_code']!=0 and 'not ok'in(E/('mutant-'+m['name']+'.log')).read_text()
for file,header,standard in [('header.json','hpp','-std=c++14'),('header-c.json','h','-std=c11')]:
 h=json.loads((E/file).read_text());assert h['exit_code']==0 and standard in h['command'] and h['header_sha256']==r['source_sha256']['app/paint/gimpmypaintcoreundo.'+header]
assert '-fno-rtti' in json.loads((E/'header.json').read_text())['command']
contract=json.loads((E/'gegl-contract.json').read_text());assert contract['library_sha256'] and 'gegl_buffer_signal_connect' in contract['installed_header_contract'] and 'accumulated' in contract['installed_header_contract']
reg=[json.loads(v)for v in(E/'regression.json').read_text().splitlines()];assert len(reg)==2 and all(q['result']=='OK' and 'env'not in q for q in reg)
assert sorted(len(re.findall(r'^ok \d+ ',q['stdout'],re.M))for q in reg)==[11,18]
assert '1080x1920x24, authenticated accepted, empty authority rejected' in(E/'regression-portrait.log').read_text()
if a.check_owned_source or a.check_source:
 for n,h in json.loads((E/'regression-source-sha256.json').read_text()).items():assert sha((R/n).read_bytes())==h,n
red=json.loads((E/'buffer-observer-red.json').read_text());assert red['result']=='FAIL' and 'historical-state=1 live-pixel=2' in red['stdout'] and 'buffer-observer-coherence' in red['stdout']
print('PASS typed native stroke Undo:16 normal+9-unit ASan16,4 causal mutants,C11/C++14 headers;real controller pairing remains open')
