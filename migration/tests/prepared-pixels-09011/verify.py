#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,re,tarfile,subprocess
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');p.add_argument('--check-owned-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in json.loads((E/'artifact-sha256.json').read_text()).items():assert sha((E/n).read_bytes())==h,n
r=json.loads((E/'report.json').read_text());assert r['task']=='09.011/native-pixel-preparation' and not r['publishes_history'] and not r['paired_stroke_undo_complete']and not r['rendered_ui_evidence']and r['oncanvas_enabled']
assert r['native_tap']==15 and r['sanitizer_tap']==15 and r['native_precision_cells']==24
with tarfile.open(E/'tested-sources.tar.gz')as t:
 for n,h in r['source_sha256'].items():
  assert sha(t.extractfile(n).read())==h,n
  if a.check_source:assert sha((R/n).read_bytes())==h,n
  if a.check_owned_source:
   if n in ['app/core/gimpimage-undo.c','app/core/gimpimage-undo.h']:
    assert sha(subprocess.check_output(['git','show','1436f90961ce7b78348d4d0c5fd86c71a96e5f9e:'+n],cwd=R))==h,n
   else:assert sha((R/n).read_bytes())==h,n

 production=t.extractfile('app/paint/painter-mypaint-surface/paint-core-pixels.cpp').read().decode()
 for forbidden in ['gimp_image_undo_','gimp_paint_core_finish','gimp_paint_core_cancel','g_hash_table_steal','g_hash_table_remove']:assert forbidden not in production,forbidden
 fixture=t.extractfile('app/tests/test-painter-prepared-pixels.cpp').read().decode();assert '"painter-canvas-ui",TRUE' in fixture and 'g_assert_true(enabled)'in fixture
rows=[json.loads(s)for s in(E/'native.json').read_text().splitlines()];assert len(rows)==1 and rows[0]['result']=='OK'and'env'not in rows[0]
for s in [rows[0]['stdout'],(E/'asan-portrait.log').read_text(),(E/'restored-portrait.log').read_text()]:
 assert len(re.findall(r'^ok \d+ ',s,re.M))==15 and s.count('prepared native precision=')==24
 for n in ['redo-rollback','retained-lifetime','allocation-reentry','same-buffer-extent','endpoint-toggle-detach','endpoint-toggle-destroy-image']:assert n in s
for n in ['normal-portrait.log','asan-portrait.log','restored-portrait.log']:assert '1080x1920x24, authenticated accepted, empty authority rejected'in(E/n).read_text()
b=json.loads((E/'asan-build.json').read_text());assert len(b['instrumented_sources'])==6 and not b['changed_during_build']and b['mutant']is None and not b['vptr_sanitizer']and not b['leak_detection']
for n,h in b['sources_sha256'].items():assert r['source_sha256'][n]==h,n
assert len(r['mutants'])==3
for m in r['mutants']:assert m['exit_code']!=0 and 'not ok'in(E/('mutant-'+m['name']+'.log')).read_text()
h=json.loads((E/'header.json').read_text());assert h['exit_code']==0 and '-fno-rtti'in h['command']and '-std=c++14'in h['command']and h['header_sha256']==r['source_sha256']['app/paint/painter-mypaint-surface/paint-core-pixels.hpp']
red=json.loads((E/'wrapper-red.json').read_text());assert red['result']=='FAIL' and 'wrapper_allocation_reentry' in red['stderr']
diagnostic=json.loads((E/'endpoint-before.json').read_text());assert diagnostic['result']=='OK' and 'env'not in diagnostic
print('PASS prepare-only native pixels:15normal+6-unit ASan15,24precision cells,3mutants,exact retained baseline/redo/reentry;paired Undo remains open')
