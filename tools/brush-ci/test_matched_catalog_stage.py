"""Execute the exact catalog bridge with the real pinned staging helper."""
import hashlib,json,runpy,struct,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
HERE=Path(__file__).resolve().parent
HELPER=HERE.parent/'run-painter-frozen-aggregate.py'
EXPECTED='4cdc8a36be383487c4cca06fc173f8d3c2b949370318b66fd15f22548eecfada'
def bridge():
 text=(HERE/'run_matched_preclick.sh').read_text()
 marker='"$run/logs/$cell-catalog-stage.log" 2>&1 <<\'PY\'\n'
 return text.split(marker,1)[1].split('\nPY\n',1)[0]
def fixture(build):
 (build/'meson-info').mkdir(parents=True);prefix=build.parent/'prefix'
 (build/'config.h').write_text('#define GETTEXT_PACKAGE "gimp30"\n#define LOCALEDIR '+json.dumps(str(prefix/'share/locale'))+'\n#define GIMPPLUGINDIR '+json.dumps(str(prefix/'lib/gimp/3.0'))+'\n')
 h=b'Content-Type: text/plain; charset=UTF-8\n';mo=struct.pack('<7I',0x950412de,0,1,28,36,0,0)+struct.pack('<4I',0,44,len(h),45)+b'\0'+h+b'\0'
 plan={};installed={};targets=[]
 for domain in ('gimp30','gimp30-std-plug-ins','org.gimp.extension.goat-exercises'):
  src=build/'po/fr/LC_MESSAGES'/(domain+'.mo');src.parent.mkdir(parents=True,exist_ok=True);src.write_bytes(mo)
  dest=('share/locale/' if domain.startswith('gimp30') else 'lib/gimp/3.0/extensions/org.gimp.extension.goat-exercises/locale/')+'fr/LC_MESSAGES/'+domain+'.mo'
  plan[str(src)]={'destination':'{prefix}/'+dest,'tag':'i18n'};installed[str(src)]=str(prefix/dest)
  targets.append({'id':domain+'-fr','filename':[str(src)],'installed':True,'install_filename':[str(prefix/dest)],'build_by_default':True})
 for name,data in [('intro-buildoptions.json',[{'name':'prefix','value':str(prefix)},{'name':'datadir','value':'share'}]),('intro-install_plan.json',{'targets':plan}),('intro-installed.json',installed),('intro-targets.json',targets)]:
  (build/'meson-info'/name).write_text(json.dumps(data))
class CatalogBridge(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name);self.helper=self.root/'helper.py';self.helper.write_bytes(HELPER.read_bytes());self.build=self.root/'build';fixture(self.build);self.dest=self.root/'locale'
 def execute(self):
  with patch.object(sys,'argv',['bridge',str(self.helper),str(self.build),str(self.dest)]):exec(compile(bridge(),'<catalog-bridge>','exec'),{})
 def test_real_helper_missing_field_is_witnessed(self):
  self.assertEqual(hashlib.sha256(self.helper.read_bytes()).hexdigest(),EXPECTED)
  original=runpy.run_path(str(self.helper))['stage_build_locales'](self.build,self.root/'original-locale')
  self.assertNotIn('helper_sha256',original)
  self.execute();result=json.loads(self.dest.with_name('locale.json').read_text())
  self.assertEqual(result.pop('helper_sha256'),EXPECTED);self.assertEqual(result,original)
  for name,row in result['catalogs'].items():self.assertEqual(hashlib.sha256((self.dest/name).read_bytes()).hexdigest(),row['sha256'])
 def test_modified_helper_rejected_before_execution(self):
  self.helper.write_text('raise RuntimeError("must not execute")')
  with self.assertRaisesRegex(SystemExit,'identity rejected'):self.execute()
  self.assertFalse(self.dest.exists())
 def test_helper_changed_during_staging_rejected(self):
  actual=runpy.run_path(str(self.helper))['stage_build_locales']
  def stage(*args):
   result=actual(*args);self.helper.write_text('changed');return result
  with patch.object(runpy,'run_path',return_value={'stage_build_locales':stage}),self.assertRaisesRegex(SystemExit,'helper or result changed'):self.execute()
  self.assertFalse(self.dest.with_name('locale.json').exists())
 def test_forged_producer_field_rejected(self):
  with patch.object(runpy,'run_path',return_value={'stage_build_locales':lambda *a:{'helper_sha256':EXPECTED}}),self.assertRaisesRegex(SystemExit,'helper or result changed'):self.execute()
 def test_symlinked_helper_rejected(self):
  target=self.root/'target.py';self.helper.rename(target);self.helper.symlink_to(target)
  with self.assertRaisesRegex(SystemExit,'identity rejected'):self.execute()
if __name__=='__main__':unittest.main()
