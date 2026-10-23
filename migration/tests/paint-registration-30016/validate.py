#!/usr/bin/env python3
import argparse,json,pathlib,hashlib,re,tarfile,struct,csv,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text())
command=[sys.executable,str(E.parent/'adapter-removal/features-entry-08018/validate.py')]
if a.check_source:command.append('--check-source')
subprocess.run(command,check=True)
for file in ['normal-results.json','asan-results.json']:
 rows=read(file);assert len(rows)==1 and rows[0]['returncode']==0
 assert len(re.findall(r'^ok \d+ ',rows[0]['stdout'],re.M))==9
counts={'layer-presets-ui':18,'painter-perspective-ui':24,'feature-lifecycle':11,'preset-gui-entries':6};rows=read('regression-results.json');assert len(rows)==4
for row in rows:
 assert row['returncode']==0 and len(re.findall(r'^ok \d+ ',row['stdout'],re.M))==counts[row['name'].split(' / ')[-1]]
mutations=read('native-mutations.json');assert len(mutations)==2
for row in mutations:assert row['returncode']!=0 and ('ref_count' if row['mutation']=='retain-transferred-reference' else 'failed') in row['stdout']+row['stderr']
assert 'undefined reference to `gimp_paint_register' in (E/'missing-symbol.log').read_text() and int((E/'missing-symbol.status').read_text())!=0
h=read('header-compile-result.json');assert h['status']=='PASS' and h['compiled_probes']==423
b=read('asan-build.json');assert len(b['instrumented_sources'])==7 and not b['changed_during_build']
with tarfile.open(E/'tested-sources.tar.gz') as tar:
 for row in read('source-manifest.json'):
  assert hashlib.sha256(tar.extractfile(row['path']).read()).hexdigest()==row['sha256']
  if a.check_source:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
d=read('original-five-duties.json');assert not d['original_task_completed'] and len(d['original'])==5
rows=list(csv.DictReader((R/'migration/inventory/hunk-wbs.tsv').open(),delimiter='\t'));assert all(x in rows for x in d['original'])
assert struct.unpack('>II',(E/'paint-prefs-oncanvas-portrait.png').read_bytes()[16:24])==(1080,1920)
print('PASS: original consuming registration native9/sanitizer9; 423 headers; 2 mutants; UI18+24 and prior entry11+6; five exact duties; source identities')
