#!/usr/bin/env python3
import importlib.util,json,pathlib,shutil,tarfile,tempfile,unittest
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
spec=importlib.util.spec_from_file_location('gate',R/'migration/tests/persistence_unsafe_undo_successor.py');gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
class Successor(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=pathlib.Path(self.temp.name);shutil.copytree(P,self.root/gate.PACKAGE);self.r=json.loads((P/'transition.json').read_text())
  with tarfile.open(P/'sources.tar.gz')as t:
   self.before=t.extractfile('predecessor/'+gate.PATH).read();self.after=t.extractfile('successor/'+gate.PATH).read()
   for p in [gate.PATH,self.r['header_path']]:
    f=self.root/p;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(t.extractfile('successor/'+p).read())
  p=self.root/self.r['runtime_package']/'artifact-sha256.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((R/self.r['runtime_package']/'artifact-sha256.json').read_bytes())
 def tearDown(self):self.temp.cleanup()
 def resolve(self):return gate.resolve_source(self.root,self.r['predecessor_sha256'],self.r['successor_sha256'])
 def test_exact(self):self.assertEqual(self.resolve(),self.r);gate.exact_change(self.before,self.after)
 def test_saved_property_changed(self):
  changed=self.after.replace(b'g_param_spec_int ("width",',b'g_param_spec_int ("lost-width",');self.assertNotEqual(changed,self.after)
  with self.assertRaises(AssertionError):gate.exact_change(self.before,changed)
 def test_header_layout_changed(self):
  p=self.root/self.r['header_path'];p.write_bytes(p.read_bytes()+b'\n/* unknown layout edit */')
  with self.assertRaises(AssertionError):self.resolve()
 def test_unreviewed_source(self):
  p=self.root/gate.PATH;p.write_bytes(p.read_bytes()+b'\n/* unrelated source drift */')
  with self.assertRaises(AssertionError):self.resolve()
 def test_missing_transition(self):
  p=self.root/gate.PACKAGE/'transition.json';p.write_bytes(b'{}')
  with self.assertRaises(AssertionError):self.resolve()
 def test_wrong_endpoints(self):
  with self.assertRaises(AssertionError):gate.resolve_source(self.root,'0'*64,self.r['successor_sha256'])
  with self.assertRaises(AssertionError):gate.resolve_source(self.root,self.r['predecessor_sha256'],'0'*64)
 def test_runtime_proof_changed(self):
  p=self.root/self.r['runtime_package']/'artifact-sha256.json';p.write_bytes(p.read_bytes()+b'\n')
  with self.assertRaises(AssertionError):self.resolve()
if __name__=='__main__':unittest.main()
