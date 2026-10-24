#!/usr/bin/env python3
import pathlib,json,hashlib,tarfile,csv,re,gzip,importlib.util
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(b):return hashlib.sha256(b).hexdigest()
def check(p=P):
 for n,h in json.loads((p/'manifest.json').read_text()).items():assert sha((p/n).read_bytes())==h,n
 a=json.loads((p/'audit.json').read_text());assert a['tasks']==['23.002']and a['status']=='OPEN'and a['closed_original_duties']==0 and len(a['rows'])==52
 current={r['work_id']:r for r in csv.DictReader((R/'migration/inventory/legacy-port-work-items.tsv').open(),delimiter='\t')}
 for row in a['rows']:
  assert row['acceptance']=='初期採色と更新が一致'
  assert sha((R/'migration/tests/smudge-initial-sampling'/row['payload']).read_bytes())==row['source_sha256']
  assert {(x['work_id'],x['wbs_task'])for x in current.values()if x['source_id']==row['source_id']and x['work_id']!=row['work_id']}=={(x['work_id'],x['task'])for x in row['other_whole_hunk_owners']}
  assert all(current[row['work_id']][k]==v for k,v in row.items() if k not in ['payload','other_whole_hunk_owners'])
 seal=json.loads((p/'final/source-before-build.json').read_text());assert len(seal)==71
 with tarfile.open(p/'final/tested-sources.tar.gz','r:gz')as t:
  assert set(t.getnames())==set(seal);src={n:t.extractfile(n).read()for n in seal}
  for n,h in seal.items():assert sha(src[n])==h,n
 for n in ['app/paint/gimppaintersmudge.cpp','app/tests/test-gimp-painter-smudge.cpp','app/tests/test-painter-smudge-center-matrix.inc','migration/tests/run_smudge_center_sanitizers.py']:
  assert sha((R/n).read_bytes())==seal[n],n
 held=json.loads((p/'final/held-source-isolation.json').read_text())['rows'];assert len(held)==10
 for row in held:
  if row.get('native_exists')is False:assert row['path']not in seal
  else:assert seal[row['path']]==row['sha256']
 options=next(x for x in held if x['path'].endswith('/gimp-painter-options.cpp'));assert options['git_blob']=='12fd73dc406128bc94167ee3444caa05cbdd9a9d'
 old=(p/'final/expected-red/original.cpp').read_bytes();candidate=(p/'final/candidate.cpp').read_bytes();assert candidate==src['app/paint/gimppaintersmudge.cpp'];assert hashlib.sha1(b'blob '+str(len(old)).encode()+b'\0'+old).hexdigest()=='a9506cf44a3f2fd2d22f39771e5e2179e586efac'
 block='''    // Native solidify returns Y float, while this legacy compositor consumes
    // byte coverage. Paper preserves its input format; normalize only here.
    Temp byte_mask;
    if (gimp_temp_buf_get_format (mask) != babl_format ("Y u8")) {
      const int width = gimp_temp_buf_get_width (mask), height = gimp_temp_buf_get_height (mask);
      byte_mask.reset (gimp_temp_buf_new (width, height, babl_format ("Y u8")));
      babl_process (babl_fish (gimp_temp_buf_get_format (mask), babl_format ("Y u8")),
                    gimp_temp_buf_get_data (mask), gimp_temp_buf_get_data (byte_mask.get ()),
                    std::size_t (width) * height);
      mask = byte_mask.get ();
    }
'''
 assert candidate.decode().count(block)==1 and candidate.decode().replace(block,'')==old.decode()
 assert candidate.index(b'gimp_brush_core_texturize_mask')<candidate.index(block.encode())<candidate.index(b'const auto* mask_data')
 redseal=json.loads((p/'final/expected-red/source.json').read_text());assert set(redseal)==set(seal);assert {n for n in seal if seal[n]!=redseal[n]}=={'app/paint/gimppaintersmudge.cpp'};assert redseal['app/paint/gimppaintersmudge.cpp']==sha(old)
 red=json.loads((p/'final/expected-red/result.json').read_text());assert red['result']=='FAIL'and red['returncode']!=0 and 'expected=130 actual=220'in red['stdout'];assert re.fullmatch('[a-f0-9]{64}',red['executable_sha256'])
 def native(out):
  assert len(re.findall(r'^ok \d+ /',out,re.M))==25 and 'not ok'not in out and '# SKIP'not in out
  rows=re.findall(r'center scene size=(\d+) paper=(\d+) channels=(\d+) entry=(\d+) rate=(\d+) callbacks=(\d+) updates=(\d+) outside=(\d+)',out)
  assert len(rows)==192 and len({tuple(x[:5])for x in rows})==192
  assert {tuple(map(int,x[:5]))for x in rows}=={(s,p,c,e,r)for s in [17,18]for p in [0,128]for c in range(1,5)for e in range(4)for r in [0,50,100]}
  assert sum(int(x[6])for x in rows)==1008 and all(int(x[6])>1 for x in rows)
  assert all((int(x[7])>0)==(int(x[3])==2)for x in rows)
  assert 'center matrix192 scenes; actual updated centers=1008; opaque alpha only'in out
 normal=json.loads((p/'final/normal.json').read_text());assert len(normal)==4 and all(r['result']=='OK'and r['returncode']==0 and r['duration']<60 for r in normal)
 native(next(r['stdout']for r in normal if r['name'].endswith('/ gimp-painter-smudge')))
 for r in normal:
  if r['name'].endswith('-strokes'):assert '2952800 bytes, 193 records'in r['stdout']
  if r['name'].endswith('-pixels'):assert '891487'in r['stdout']and '1024'in r['stdout']
 assert '1080x1920x24, authenticated accepted, empty authority rejected'in(p/'final/normal.log').read_text()
 paper=json.loads((p/'final/normal-paper.json').read_text());assert len(paper)==2 and {tuple(x['args'])for x in paper}=={(),('owned',)}
 ph=sha(gzip.decompress(src['migration/fixtures/legacy-paper-smudge/pixels.tsv.gz']))
 for row in paper:assert row['exit_code']==0 and row['exact']and row['records']==193 and row['bytes']==2952800 and row['duration']<60 and row['expected_sha256']==row['normalized_sha256']==ph
 san=json.loads((p/'final/focused-native.json').read_text());assert san['scope'].startswith('18 focused native units')and len(san['sources_sha256'])==47 and not san['changed_during_run']
 for n,h in san['sources_sha256'].items():assert seal[n]==h,n
 assert 'app/config/gimppainterfilterconfig.cpp'in san['sources_sha256']
 assert all('-fno-sanitize'not in ' '.join(c)for c in san['commands']);assert sum('-fsanitize=address,undefined,float-cast-overflow'in c for c in san['commands'])==20
 assert all(re.fullmatch('[a-f0-9]{64}',r['executable_sha256'])for r in san['runs'])
 assert len(san['runs'])==5 and all(r['exit_code']==0 for r in san['runs']);native(san['runs'][0]['stdout'])
 assert {tuple(r['args'])for r in san['runs'][1:]}=={(),('owned',),('paper',),('paper','owned')}
 for r in san['runs'][1:]:assert '2952800 bytes,193 records'in r['stdout']
 pixels=json.loads((p/'final/focused-pixels.json').read_text());assert pixels['exit_code']==0 and pixels['exact_legacy_match']and pixels['cases']==1024 and pixels['expected_bytes']==891487 and not pixels['changed_during_run']
 assert pixels['sanitizers']==['address','undefined','float-cast-overflow'] and not pixels['leak_detection']
 assert pixels['expected_sha256']==sha(gzip.decompress(src['migration/fixtures/legacy-smudge-pixels/pixels.tsv.gz']))
 for n,h in pixels['sources_sha256'].items():assert seal[n]==h
 assert 'center=130 opacity=1 rate=0.5'in(p/'diagnostics/diagnostic7/run.log').read_text()and 'center=0'in(p/'diagnostics/diagnostic7/run.log').read_text()
 spec=importlib.util.spec_from_file_location('old_center',R/'migration/fixtures/legacy-smudge-center/verify-source.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);m.verify()
 print('PASS: exact hard-mask red/green;192 scenes/1008 centers;25 native;48x4 old strokes;1024 kernels;ordinary focused sanitizers;10 held;52 duties OPEN')
if __name__=='__main__':check()
