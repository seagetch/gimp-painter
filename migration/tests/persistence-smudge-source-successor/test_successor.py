#!/usr/bin/env python3
import pathlib,sys,tarfile,tempfile,shutil,unittest
ROOT=pathlib.Path(__file__).resolve().parents[3];sys.path.insert(0,str(ROOT/'migration/tests'))
import persistence_smudge_source_successor as gate
class Tests(unittest.TestCase):
 def setUp(self):
  with tarfile.open(ROOT/gate.PACKAGE/'sources.tar.gz') as t:self.before=t.extractfile('predecessor.cpp').read();self.after=t.extractfile('successor.cpp').read()
 def test_exact_and_saved_field_controls(self):
  gate.exact_change(self.before,self.after)
  for old,new in [(b'double rate = 50',b'double rate = 51'),(b'bool blending = false',b'bool blending = true'),(b'void options_set (',b'void options_set_changed ('),(b'void options_get (',b'void options_get_changed ('),(b'void motion (GimpPaintCore* core,',b'void motion_changed (GimpPaintCore* core,'),(b'void interpolate (GimpPaintCore* core,',b'void interpolate_changed (GimpPaintCore* core,')]:
   mutant=self.after.replace(old,new,1);self.assertNotEqual(mutant,self.after)
   with self.assertRaises(AssertionError):gate.exact_change(self.before,mutant)
 def test_endpoint_source_proof_controls(self):
  with tempfile.TemporaryDirectory() as tmp:
   root=pathlib.Path(tmp);shutil.copytree(ROOT/gate.PACKAGE,root/gate.PACKAGE);(root/gate.PATH).parent.mkdir(parents=True);(root/gate.PATH).write_bytes(self.after)
   row=gate.reviewed(root);gate.resolve_source(root,row['predecessor_sha256'],row['successor_sha256'])
   for a,b in [('0'*64,row['successor_sha256']),(row['predecessor_sha256'],'0'*64)]:
    with self.assertRaises(AssertionError):gate.resolve_source(root,a,b)
   for data in [self.after+b'\n',self.after.replace(b'void motion (',b'void motion_altered (',1)]:
    (root/gate.PATH).write_bytes(data)
    with self.assertRaisesRegex(AssertionError,'unreviewed committed Smudge'):gate.reviewed(root)
   (root/gate.PATH).write_bytes(self.after)
   for name in gate.PINS:
    p=root/gate.PACKAGE/name;data=p.read_bytes();p.write_bytes(data+b' ')
    with self.assertRaisesRegex(AssertionError,'Smudge proof changed'):gate.reviewed(root)
    p.write_bytes(data)
if __name__=='__main__':unittest.main(verbosity=2)
