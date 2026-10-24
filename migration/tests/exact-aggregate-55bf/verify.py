#!/usr/bin/env python3
import hashlib,json,pathlib,re,struct,tarfile
COMMIT='55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94'
ARCHIVE='7c4aca51c5e2623e69836005f0660e3059e6840535ec74cd283b7fd16d081ba2'
def validate(rows,summary,extras,audit):
 assert len(rows)==44 and all(r['result']=='OK' for r in rows)
 assert sum(len(re.findall(r'^ok \d+',r.get('stdout')or'',re.M))for r in rows)==478
 assert summary['whole_selected_aggregate_pass'] and summary['completed_ok_tap_cases']==478 and summary['selected_extra_total']==54
 assert summary['selected_extra_cases']=={'red-eye-exchange-xcf':7,'red-eye-portrait':1,'filter-selected-no-edge':43,'graph-ui':3}
 assert len(extras)==4 and all(r.get('returncode')==0 for r in extras)
 assert audit['default_enabled'] is False and audit['not_blanket_oncanvas_44_targets'] is True
 assert audit['missing_explicit_enable']==['painter-smudge-ui','painter-save-continuation (includes Fill scene)']
 assert audit['unused_environment_not_used_as_proof'] is True
 for name,count in [('painter-save-continuation',13),('device-name-identity',2),('painter-editor-named-entry',3)]:
  row=next(r for r in rows if r['name'].endswith(' / '+name));assert len(re.findall(r'^ok \d+',row['stdout'],re.M))==count

def verify(root):
 root=pathlib.Path(root);assert hashlib.sha256((root/'evidence.tar.gz').read_bytes()).hexdigest()==ARCHIVE
 with tarfile.open(root/'evidence.tar.gz') as t:
  prefix='exact-'+COMMIT+'/'
  read=lambda n:json.loads(t.extractfile(prefix+n).read())
  assert t.extractfile(prefix+'source-commit.txt').read().decode().strip()==COMMIT
  pins=read('artifacts-sha256.json');assert len(pins)==50
  for n,h in pins.items():assert hashlib.sha256(t.extractfile(prefix+n).read()).hexdigest()==h,n
  validate(read('results.json'),read('aggregate-summary.json'),read('extra-ui-results.json'),read('oncanvas-scope-audit.json'))
  assert read('final-source-seal.json')=={'checked_files':19165,'changed':[]}
  assert read('postrun-external-data-seal.json')=={'checked_files':1714,'changed':[]}
  assert len(read('native-executable-sha256.json'))==42
  held=read('postrun-held-exclusions.json');assert held=={'held_scopes':7,'providers':11,'all_unchanged':True,'untracked_size_domain_candidate_absent':True}
  dimensions=read('image-dimensions.json');assert len(dimensions)==24
  for n,d in dimensions.items():
   expected=[1040,1800] if n.startswith('physical-screenshots/') else [1080,1920];assert d==expected
   b=t.extractfile(prefix+n).read(24);assert b[:8]==b'\x89PNG\r\n\x1a\n' and list(struct.unpack('>II',b[16:24]))==expected
  assert b'1080x1920x24, authenticated accepted, empty authority rejected' in t.extractfile(prefix+'tests.log').read()
 print('PASS: exact55bf44 targets/478 cases plus54 selected extras; oncanvas gaps explicitly retained')
if __name__=='__main__':verify(pathlib.Path(__file__).resolve().parent)
