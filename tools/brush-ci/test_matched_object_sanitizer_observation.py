import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('provenance',Path(__file__).with_name('matched_provenance.py'))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class ObjectObservation(unittest.TestCase):
 def error(self,args,cell='baseline-sanitizer',obj='app/core/libappcore.a.p/example.c.o',source='source/app/core/example.c'):
  recipe='\0'.join(args)
  with self.assertRaises(m.ObjectSanitizerError) as caught:
   m.validate_object_sanitizer(args,recipe,cell,obj,source,12,1500)
  self.assertEqual(str(caught.exception),'object-sanitizer-mismatch')
  return caught.exception
 def test_missing_flags(self):
  e=self.error(['cc','-c','example.c']);d=e.diagnostic
  self.assertEqual(d['sanitizer_arguments'],{'labels':[],'truncated':False})
  self.assertEqual(d['compile_row_index'],12);self.assertEqual(d['compile_row_count'],1500)
  self.assertEqual(d['recipe_sha256'],m.digest('cc\0-c\0example.c'.encode()))
 def test_split_flags_still_fail(self):
  d=self.error(['cc','-fsanitize=address','-fsanitize=undefined']).diagnostic
  self.assertEqual(d['sanitizer_arguments']['labels'],['enable-address-only','enable-undefined-only'])
 def test_normal_sanitizer_flag_still_fails(self):
  self.error(['cc','-fsanitize=address,undefined'],'baseline-normal')
 def test_predicate_exact_equivalence(self):
  samples=[[],['-fsanitize=address,undefined'],['-fsanitize=undefined,address'],['-fsanitize=address'],
   ['-fsanitize=address','-fsanitize=undefined'],['-fsanitize=address,undefined','-fno-sanitize=all'],
   ['-DNOTE=-fsanitize=address,undefined'],['-fsanitize=address,undefined,thread']]
  for cell in m.CELLS:
   for args in samples:
    recipe='\0'.join(args)
    old=(('-fsanitize=address,undefined' in recipe or '-fsanitize=undefined,address' in recipe) if cell.endswith('sanitizer') else '-fsanitize=' not in recipe)
    try:m.validate_object_sanitizer(args,recipe,cell,'x.o','source/x.c',0,1);new=True
    except m.ObjectSanitizerError:new=False
    self.assertEqual(old,new,(cell,args))
 def test_private_arguments_not_exported(self):
  d=self.error(['cc','-DSECRET=/private/key','-fsanitize=/private/key','-fno-sanitize=/private/key','-fno-sanitize=vptr']).diagnostic
  text=json.dumps(d)
  self.assertNotIn('/private',text);self.assertNotIn('SECRET',text)
  self.assertEqual(d['sanitizer_arguments']['labels'],['enable-other','disable-other','disable-vptr'])
 def test_argument_bound(self):
  d=self.error(['-fno-sanitize=all']*1000).diagnostic
  self.assertEqual(len(d['sanitizer_arguments']['labels']),16);self.assertTrue(d['sanitizer_arguments']['truncated'])
  self.assertLess(len(json.dumps(d)),2048)
 def test_path_bounds(self):
  for value in ['/private/key','../secret','x/../secret','x//secret','x\nsecret','x'*241,'./x']:
   d=self.error([],obj=value,source=value).diagnostic
   self.assertEqual(d['object'],'unavailable');self.assertEqual(d['source'],'unavailable')
 def test_main_retains_failure_and_payload(self):
  e=self.error(['cc','-fsanitize=address'])
  with tempfile.TemporaryDirectory() as td:
   out=Path(td)/'result.json'
   argv=['matched_provenance.py','build-identity','--source',td,'--build',td,'--deps',td,'--cell','baseline-sanitizer','--output',str(out)]
   with patch.object(sys,'argv',argv),patch.object(m,'build_identity',side_effect=e):
    self.assertEqual(m.main(),1)
   report=json.loads(out.read_text());self.assertFalse(report['verified']);self.assertFalse(report['acceptance_eligible'])
   self.assertEqual(report['errors'],['object-sanitizer-mismatch']);self.assertEqual(report['object_sanitizer_failure'],e.diagnostic)

if __name__=='__main__':unittest.main()
