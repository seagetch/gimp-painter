#!/usr/bin/env python3
"""Evidence metadata redaction must preserve every test result/diagnostic."""
import json
from pathlib import Path
import tempfile
import unittest
from sanitize_filter_evidence import sanitize_file, sanitize_json_text, sanitize_text, sanitize_value
class Evidence(unittest.TestCase):
 def row(self):
  return {'name':'native-case','result':'OK','stdout':'ok 1 /test\n','stderr':'diagnostic retained',
          'env':{'UI_TEST':'yes','ASAN_OPTIONS':'detect_leaks=0','PRIVATE_TEST_VALUE':'dummy',
                 'CODEX_'+'SESSION_ID':'synthetic-id'}}
 def test_jsonl_extension(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'proof.jsonl';p.write_text('\n'.join(json.dumps(self.row()) for _ in range(2)))
   sanitize_file(p);rows=[json.loads(s) for s in p.read_text().splitlines()]
   self.assertEqual(rows,[sanitize_value(self.row())]*2)
 def test_nested_report(self):
  raw={'runs':[self.row(),{'nested':{'environment':self.row()['env']}}]}
  out=json.loads(sanitize_json_text(json.dumps(raw,indent=2)))
  self.assertEqual(out['runs'][0]['stdout'],self.row()['stdout']);self.assertEqual(out['runs'][0]['stderr'],self.row()['stderr'])
  self.assertEqual(set(out['runs'][1]['nested']['environment']),{'UI_TEST','ASAN_OPTIONS'})
 def test_json_array(self):self.assertEqual(json.loads(sanitize_json_text(json.dumps([self.row()]))),[sanitize_value(self.row())])
 def test_single_jsonl_row(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'single.jsonl';p.write_text(json.dumps(self.row()));sanitize_file(p)
   # A one-row JSONL file must remain line-delimited, too.
   self.assertEqual(len(p.read_text().splitlines()),1)
 def test_multiline_environment_text(self):
  raw='Preamble\nInherited environment: PRIVATE=\'line one\nline two\'\n============ 1/2 ============\nstdout\nok 1 /test\nstderr\ndiagnostic\n'
  clean=sanitize_text(raw);self.assertNotIn('line two',clean);self.assertIn('ok 1 /test\nstderr\ndiagnostic',clean)
 def test_unknown_frame_refused(self):
  with self.assertRaises(ValueError):sanitize_text('Inherited environment: PRIVATE=dummy\nunknown format')
 def test_idempotent(self):
  once=sanitize_json_text(json.dumps({'nested':self.row()}));self.assertEqual(sanitize_json_text(once),once)
if __name__=='__main__':unittest.main()
