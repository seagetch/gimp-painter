#!/usr/bin/env python3
"""Negative coverage for generated PDB public-mode numeric identity."""
import importlib.util
from pathlib import Path
import unittest
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('modes',root/'tools/check_painter_public_modes.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Modes(unittest.TestCase):
 def setUp(self):
  self.public=m.modes((root/'libgimp/gimpenums.h').read_text());self.pdb=m.pdb_modes((root/'pdb/enums.pl').read_text())
 def test_match(self):self.assertEqual([],m.check_pdb(self.pdb,self.public));self.assertEqual(72,len(self.pdb[0]))
 def test_old_shift(self):
  values=dict(self.pdb[0]);values['GIMP_LAYER_MODE_PAINTER_ERASE']=63
  self.assertTrue(m.check_pdb((values,0),self.public))
 def test_hidden_mode(self):
  values=dict(self.pdb[0]);values['GIMP_LAYER_MODE_ANTI_ERASE']=63
  self.assertTrue(m.check_pdb((values,0),self.public))
 def test_contiguity(self):self.assertTrue(m.check_pdb((self.pdb[0],1),self.public))
if __name__=='__main__':unittest.main()
