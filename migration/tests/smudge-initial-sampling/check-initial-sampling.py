#!/usr/bin/env python3
import csv,hashlib,json,pathlib,re,tarfile,importlib.util,gzip
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 a=json.loads((p/'audit.json').read_text());assert a['tasks']==['23.002'] and a['status']=='OPEN' and a['closed_original_duties']==0 and len(a['rows'])==52
 rows=list(csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t'));lookup={r['work_id']:r for r in rows}
 for r in a['rows']:
  assert all(lookup[r['work_id']][k]==v for k,v in r.items() if k not in ['payload','other_whole_hunk_owners'])
  assert sha((p/r['payload']).read_bytes())==r['source_sha256']
  assert {(x['work_id'],x['wbs_task']) for x in rows if x['source_id']==r['source_id'] and x['work_id']!=r['work_id']}=={(x['work_id'],x['task']) for x in r['other_whole_hunk_owners']}
 seal=json.loads((p/'final/source-before-normal-build.json').read_text());assert len(seal)==57
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz') as t:
  assert set(t.getnames())==set(seal);source={n:t.extractfile(n).read() for n in seal}
  for n,h in seal.items():assert sha(source[n])==h,n
 for n in ['app/paint/gimppaintersmudge.cpp','app/tests/test-gimp-painter-smudge.cpp','app/tests/test-painter-smudge-initial-sampling.inc','migration/tests/run_smudge_initial_sanitizers.py']:
  actual=sha((R/n).read_bytes())
  if actual!=seal[n]:
   admission=json.loads((p/'successor-source-admission.json').read_text());assert set(admission['files'])=={'app/paint/gimppaintersmudge.cpp','app/tests/test-gimp-painter-smudge.cpp'}
   assert admission['files'][n]=={'historical':seal[n],'current':actual}
   successor=R/admission['successor'];assert admission['successor']=='migration/tests/smudge-mask-format'
   assert sha((successor/'check-mask-format.py').read_bytes())==admission['checker_sha256']
   exact=json.loads((successor/'final/source-before-build.json').read_text());assert exact[n]==actual
   spec=importlib.util.spec_from_file_location('mask_successor',successor/'check-mask-format.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod);mod.check()
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==9
 for row in held:
  if row.get('native_exists') is False:assert row['path'] not in seal
  else:assert seal[row['path']]==row['sha256']
 old=(p/'final/expected-red/original.cpp').read_bytes()
 assert hashlib.sha1(b'blob '+str(len(old)).encode()+b'\0'+old).hexdigest()=='ab4418bb86256aad39ffd1910f23c90e8f18a745'
 expected=old.decode().replace('if (opacity == 0) return;','// Old start seeds the first overlapping footprint even when this dab is\n  // transparent. Later zero-opacity dabs neither resample nor update it.\n  if (opacity == 0 && state.accumulator) return;')
 marker='  const int x = int (coords.x) - (state.size - 2) / 2 - 1, y = int (coords.y) - (state.size - 2) / 2 - 1;\n  GeglRectangle accum_area'
 assert expected.count(marker)==1;expected=expected.replace(marker,'  if (opacity == 0) return;\n'+marker)
 assert source['app/paint/gimppaintersmudge.cpp']==expected.encode()==(p/'final/candidate.cpp').read_bytes()
 redseal=json.loads((p/'final/expected-red/source-before-build.json').read_text());candidate=dict(redseal);candidate['app/paint/gimppaintersmudge.cpp']=sha(expected.encode());assert candidate==seal
 assert redseal['app/paint/gimppaintersmudge.cpp']==sha(old)
 red=json.loads((p/'final/expected-red/result.json').read_text());assert red['result']=='FAIL' and red['returncode']!=0
 assert 'actual positive-pressure native Smudge dabs=4' in red['stdout'] and 'expectedR=40 actualR=220' in red['stdout']
 fixture=R/'migration/fixtures/legacy-smudge-initial-order'
 for f in fixture.iterdir():
  if f.is_file():assert sha(f.read_bytes())==seal[str(f.relative_to(R))]
 spec=importlib.util.spec_from_file_location('oldinit',fixture/'verify-source.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.check()
 def native(stdout):
  cases=[l for l in stdout.splitlines() if l.startswith('ok ') and '# SKIP' not in l];assert len(cases)==24
  assert len({l.split(' ',2)[2] for l in cases})==24 and '/painter/smudge/initial-zero-opacity' in cases[13]
  for n,x,y,v in [(0,56,16,40),(1,56,16,40),(2,24,16,220),(3,56,16,40)]:assert f'old initial seed variant={n} target=({x},{y}) expectedR={v} actualR={v}' in stdout
  assert stdout.count('actual positive-pressure native Smudge dabs=4')==3 and stdout.count('actual positive-pressure native Smudge dabs=5')==1
  assert stdout.count('initial seed changed native bytes=486')==3 and stdout.count('initial seed changed native bytes=522')==1
 normal=json.loads((p/'final/normal.json').read_text());assert len(normal)==4
 names={j['name'].split(' / ')[-1] for j in normal};assert names=={'gimp-painter-smudge','painter-smudge-legacy-strokes','painter-smudge-owned-strokes','painter-smudge-legacy-pixels'}
 for j in normal:
  assert j['result']=='OK' and j['returncode']==0 and j['duration']<60 and j['source_sha256']==seal
  assert j['xvfb']==dict(width=1080,height=1920,depth=24,authenticated=True)
  if j['name'].endswith(' / gimp-painter-smudge'):native(j['stdout'])
  elif 'pixels' in j['name']:assert '891487 bytes, 1024 cases' in j['stdout']
  else:assert '2952800 bytes, 193 records' in j['stdout']
 assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/'final/normal.log').read_text()
 executables=json.loads((p/'final/normal-executables.json').read_text());assert len(executables)==3 and all(re.fullmatch('[a-f0-9]{64}',v) for v in executables.values())
 san=json.loads((p/'final/focused-native.json').read_text());assert san['scope'].startswith('18 focused native units:') and not san['changed_during_run'] and len(san['sources_sha256'])==39
 assert san['sources_sha256'].get('app/config/gimppainterfilterconfig.cpp')==seal['app/config/gimppainterfilterconfig.cpp']
 for n,h in san['sources_sha256'].items():assert seal[n]==h,n
 assert len(san['runs'])==3 and [(r['test'],r['args']) for r in san['runs']]==[('gimp-painter-smudge',[]),('painter-smudge-trace',[]),('painter-smudge-trace',['owned'])]
 for j in san['runs']:
  assert j['exit_code']==0 and re.fullmatch('[a-f0-9]{64}',j['executable_sha256'])
  if j['test']=='gimp-painter-smudge':native(j['stdout'])
  else:assert '2952800 bytes,193 records' in j['stdout']
 assert all('-fno-sanitize' not in arg for cmd in san['commands'] for arg in cmd)
 pix=json.loads((p/'final/focused-pixels.json').read_text());assert pix['exit_code']==0 and pix['exact_legacy_match'] and pix['cases']==1024 and pix['expected_bytes']==891487 and not pix['changed_during_run'] and not pix['leak_detection']
 assert pix['sanitizers']==['address','undefined','float-cast-overflow']
 for n,h in pix['sources_sha256'].items():assert seal[n]==h
 assert sha(gzip.decompress(source['migration/fixtures/legacy-smudge-pixels/pixels.tsv.gz']))==pix['expected_sha256']
 failed=json.loads((p/'diagnostics/mixed-rtti-focused-native.json').read_text());assert any(r['exit_code']!=0 and 'invalid vptr' in r['stderr'] and 'gimp_painter_filter_config_set_spill' in r['stderr'] for r in failed['runs'])
 assert 'TRACE seed=40,120,70,255' in (p/'diagnostics/diagnostic-trace/run.log').read_text()
 print('PASS: exact corrected red/green initial seed;24 native cases;48x2 old strokes;1024 kernels;18-unit focused sanitizers;52 duties OPEN')
if __name__=='__main__':check()
