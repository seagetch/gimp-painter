#!/usr/bin/env python3
"""Synthetic admission controls plus unchanged historical-checker execution."""
import copy,csv,hashlib,io,json,pathlib,shutil,sys,tarfile,tempfile,unittest
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[4];sys.path.insert(0,str(ROOT/'tools'))
import check_painter_header_registration as checker
class Duties(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  with tarfile.open(checker.RECOVERY/'historical-checkpoint.tar.gz')as t:
   cls.original=list(csv.DictReader(io.StringIO(t.extractfile('migration/inventory/legacy-port-work-items.tsv').read().decode()),delimiter='\t'))
 def setUp(self):self.live=checker.rows(ROOT/'migration/inventory/legacy-port-work-items.tsv')
 def own(self):return next(r for r in self.live if r['wbs_task']=='04.014')
 def check(self):return checker.check_current_duties(self.original,self.live)
 def reject(self):
  with self.assertRaises(AssertionError):self.check()
 def test_current(self):self.check()
 def test_missing(self):self.live.remove(self.own());self.reject()
 def test_duplicate(self):self.live.append(copy.deepcopy(self.own()));self.reject()
 def test_not_done(self):self.own()['status']='TODO';self.reject()
 def test_retarget(self):self.own()['wbs_task']='04.015';self.reject()
 def test_changed_acceptance(self):self.own()['acceptance']='weakened';self.reject()
 def test_changed_identity(self):self.own()['source_sha256']='0'*64;self.reject()
 def test_missing_proof(self):self.own()['artifact']='-';self.reject()
 def test_unrelated_completion(self):
  row=next(r for r in self.live if r['wbs_task']!='04.014' and r['status']=='TODO');row.update(status='DONE',owner='control',artifact='proof',test='test',result='pass');self.check()
 def test_unchanged_original_checker(self):checker.check_historical()
 def test_damaged_history(self):
  with tempfile.TemporaryDirectory()as directory:
   p=pathlib.Path(directory)
   for n in ['historical-pin.json','historical-checkpoint.tar.gz']:shutil.copy2(checker.RECOVERY/n,p/n)
   with (p/'historical-checkpoint.tar.gz').open('ab')as f:f.write(b'damaged')
   with patch.object(checker,'RECOVERY',p),self.assertRaises(AssertionError):checker.check_historical()
class BuildAdmission(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=pathlib.Path(self.tmp.name);self.d=self.root/'app/painter/tests/headers';self.d.mkdir(parents=True)
  self.probes=[{'name':'probe-000.c','language':'c','body':'/* C control */\n'},{'name':'probe-001.cpp','language':'cpp','body':'/* C++ control */\n'}]
  self.manifest={'format':1,'http_enabled':True,'outer_linkage_adapter':False,'source_sha256':{'control':'1'*64}}
  self.result={'status':'PASS',**self.manifest,'compiled_probes':2,'c_probes':1,'cpp_probes':1,'libraries':{}}
  self.commands=[]
  for p in self.probes:
   (self.d/p['name']).write_text(p['body']);obj=self.root/(p['name']+'.o');obj.write_bytes(b'control-object')
   self.commands.append({'directory':str(self.root),'file':str(self.d/p['name']),'output':obj.name,'command':'cc -Werror '+('-std=c11' if p['language']=='c' else '-std=c++14')})
  for n in ['c.a','cpp.a']:
   f=self.d/n;f.write_bytes(b'control-archive');self.result['libraries'][str(f)]={'bytes':f.stat().st_size,'sha256':checker.sha(f.read_bytes())}
  self.targets=[{'name':n,'type':'static library','installed':False,'build_by_default':True} for n in ['painter-public-headers-c','painter-public-headers-cpp']]
  (self.root/'meson-info').mkdir();self.save()
 def save(self):
  for p,d in [(self.d/'header-compile-result.json',self.result),(self.d/'header-probes.json',self.manifest),(self.root/'compile_commands.json',self.commands),(self.root/'meson-info/intro-targets.json',self.targets)]:p.write_text(json.dumps(d))
 def check(self):
  with patch.object(checker,'current_specification',return_value=(self.probes,self.manifest)):return checker.check_build(self.root)
 def reject(self):
  with self.assertRaises((AssertionError,FileNotFoundError)):self.check()
 def test_consistent_synthetic_admission(self):self.check()
 def test_stale_source_fingerprint(self):self.result['source_sha256']={'control':'2'*64};self.save();self.reject()
 def test_stale_generated_manifest(self):self.save();(self.d/'header-probes.json').write_text('{}');self.reject()
 def test_modified_probe(self):(self.d/'probe-000.c').write_text('wrong');self.reject()
 def test_wrong_count(self):self.result['compiled_probes']=1;self.save();self.reject()
 def test_modified_archive(self):(self.d/'c.a').write_bytes(b'changed');self.reject()
 def test_missing_archive(self):(self.d/'c.a').unlink();self.reject()
 def test_missing_object(self):(self.root/'probe-000.c.o').unlink();self.reject()
 def test_wrong_language(self):self.commands[0]['command']='c++ -std=c++14 -Werror';self.save();self.reject()
 def test_weakened_warnings(self):self.commands[0]['command']='cc -std=c11';self.save();self.reject()
 def test_partial_werror(self):self.commands[0]['command']='cc -std=c11 -Werror=return-type';self.save();self.reject()
 def test_duplicate_vector(self):self.commands.append(copy.deepcopy(self.commands[0]));self.save();self.reject()
 def test_installed_archive(self):self.targets[0]['installed']=True;self.save();self.reject()
 def test_nondefault_archive(self):self.targets[0]['build_by_default']=False;self.save();self.reject()
if __name__=='__main__':unittest.main()
