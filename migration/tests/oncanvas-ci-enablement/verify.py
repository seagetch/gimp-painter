#!/usr/bin/env python3
import hashlib,json,pathlib,re,struct,tarfile
COMMIT='55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94'
ARCHIVE='969256dccc2d30db47acc85c32efc88768566141385055884cf00b21394b6a82'
COUNTS={'painter-smudge-ui':26,'painter-save-continuation':13,'painter-mypaint-tool':27,'paint-geometry-tool':8}
def validate(rows,landscape,negative,summary,seal):
 assert len(rows)==4 and {r['target']:r['ok_tap_cases']for r in rows}==COUNTS
 for r in rows:
  assert r['returncode']==0 and len(re.findall(r'^ok \d+',r['stdout'],re.M))==COUNTS[r['target']]
  assert '1080x1920x24, authenticated accepted, empty authority rejected' in r['stdout']
 assert len(landscape)==3 and {r['target']for r in landscape}==set(COUNTS)-{'paint-geometry-tool'}
 for r in landscape:
  assert r['returncode']==0 and r['portrait_flag'] is False
  assert len(re.findall(r'^ok \d+',r['stdout'],re.M))==1
  assert '1280x800x24, authenticated accepted, empty authority rejected' in r['stdout']
 assert negative['returncode']!=0 and negative['portrait_flag'] is True
 assert '1080' in negative['stdout']+negative['stderr'] and '1280' in negative['stdout']+negative['stderr']
 assert summary['production_commit']==COMMIT and summary['portrait_cases']==74 and summary['landscape_cases']==3
 assert summary['production_changes']==0 and summary['actual_oncanvas_enabled'] and summary['original_assertions_retained']
 assert summary['portrait_optin']=='GIMP_TEST_REQUIRE_PORTRAIT=1' and not summary['default_ci_fixed_resolution'] and not summary['new_sanitizer_claim']
 assert seal['production_commit']==COMMIT and seal['committed_sources']==19165 and seal['unchanged'] and seal['cases']==74

def validate_source(old,new,name):
 lines=iter(new.splitlines());assert all(any(x==line for x in lines)for line in old.splitlines()),name
 if name.endswith('test-painter-save-continuation.cpp'):return
 assert 'g_object_set (gimp->config, "painter-canvas-ui", TRUE, nullptr);' in new
 assert 'g_assert_true (gimp_painter_canvas_ui_get_visible (oncanvas_shell));' in new
 assert 'if (g_strcmp0 (g_getenv ("GIMP_TEST_REQUIRE_PORTRAIT"), "1") == 0) {' in new
 assert 'gdk_pixbuf_get_from_window (root, 0, 0, 1080, 1920)' not in new
 for symbol in ['oncanvas_root','root']:
  fragment='if (g_strcmp0 (g_getenv ("GIMP_TEST_REQUIRE_PORTRAIT"), "1") == 0) {\n      g_assert_cmpint (gdk_window_get_width ('+symbol+'), ==, 1080);\n      g_assert_cmpint (gdk_window_get_height ('+symbol+'), ==, 1920);\n    }'
  assert fragment in new

def verify(root,source_root=None):
 root=pathlib.Path(root);assert hashlib.sha256((root/'evidence.tar.gz').read_bytes()).hexdigest()==ARCHIVE
 with tarfile.open(root/'evidence.tar.gz') as t:
  read=lambda n:t.extractfile('oncanvas-ci/'+n).read();data=lambda n:json.loads(read(n))
  pins=data('artifacts-sha256.json')
  for n,h in pins.items():assert hashlib.sha256(read(n)).hexdigest()==h,n
  validate(data('results.json'),data('landscape-results.json'),data('portrait-flag-negative.json'),data('summary.json'),data('final-seal.json'))
  owned=data('source-pins.json');assert owned['base_commit']==COMMIT and len(owned['files'])==4
  for n,p in owned['files'].items():
   old=read('base/'+n);new=read(n)
   assert hashlib.sha256(old).hexdigest()==p['base_sha256'] and hashlib.sha256(new).hexdigest()==p['candidate_sha256']
   validate_source(old.decode(),new.decode(),n)
   if source_root:assert hashlib.sha256((pathlib.Path(source_root)/n).read_bytes()).hexdigest()==p['candidate_sha256'],n
  dims=data('image-dimensions.json');assert len(dims)==8
  for n,d in dims.items():
   expected=[1280,800] if n.startswith('landscape-') else ([1040,1800] if n.startswith('geometry-') else [1080,1920])
   b=read(n);assert d==expected and b[:8]==b'\x89PNG\r\n\x1a\n' and list(struct.unpack('>II',b[16:24]))==expected
 print('PASS: always-oncanvas test Scenes;74 portrait +3 landscape; portrait flag rejects wrong dimensions; production unchanged')
if __name__=='__main__':
 import sys
 verify(pathlib.Path(__file__).resolve().parent,sys.argv[1] if len(sys.argv)>1 else None)
