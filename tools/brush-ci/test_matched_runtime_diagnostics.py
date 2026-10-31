#!/usr/bin/env python3
"""Bounded diagnostic labels do not change validation outcomes."""
import importlib.util,json,subprocess,sys,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
# In full checkout resources lives alongside this file; isolated delta replay
# places the unchanged source on PYTHONPATH externally.
spec=importlib.util.spec_from_file_location('provenance',HERE/'matched_provenance.py');P=importlib.util.module_from_spec(spec);spec.loader.exec_module(P)
from matched_resources import ResourceError
class RuntimeDiagnostics(unittest.TestCase):
 def test_stage_success_unchanged(self):
  for stage in P.RUNTIME_STAGES:
   value=[]
   with P.runtime_stage(stage):value.append(3)
   self.assertEqual(value,[3])
 def test_private_details_not_exported(self):
  errors=[OSError('/private/secret-value'),ValueError('SECRET=token'),KeyError('/path/secret'),TypeError('private'),StopIteration('private'),subprocess.CalledProcessError(4,['secret-command'],output=b'private stdout',stderr=b'private stderr')]
  for stage in P.RUNTIME_STAGES:
   for error in errors:
    with self.assertRaises(P.RuntimeInspectionError) as caught:
     with P.runtime_stage(stage):raise error
    e=caught.exception
    self.assertEqual(str(e),'inspection-unavailable');self.assertEqual(e.diagnostic['stage'],stage)
    self.assertFalse(e.diagnostic['details_exported'])
    self.assertEqual(set(e.diagnostic),{'stage','category','details_exported'})
    self.assertNotIn('private',json.dumps(e.diagnostic));self.assertNotIn('secret',json.dumps(e.diagnostic))
 def test_evidence_rejections_retained(self):
  with self.assertRaises(P.RuntimeInspectionError) as caught:
   with P.runtime_stage('wrapper-command'):raise P.EvidenceError('runtime-binary-mismatch')
  self.assertEqual(str(caught.exception),'runtime-binary-mismatch')
  self.assertEqual(caught.exception.diagnostic['category'],'evidence-rejected')
 def test_resource_code_allowlist(self):
  for code in P.RESOURCE_FAILURE_CODES:
   with self.assertRaises(P.RuntimeInspectionError) as caught:
    with P.runtime_stage('resource-inventory'):raise ResourceError(code)
   self.assertEqual(caught.exception.diagnostic['resource_code'],code)
   self.assertEqual(str(caught.exception),'inspection-unavailable')
  for code in ('/private/secret','SECRET='+('a'*100000),'resource-file-invalid:/secret'):
   with self.assertRaises(P.RuntimeInspectionError) as caught:
    with P.runtime_stage('resource-inventory'):raise ResourceError(code)
   self.assertNotIn('resource_code',caught.exception.diagnostic)
 def test_resource_substage_is_finite(self):
  for substage in ('catalog-alias-inputs','/private/secret',None):
   error=ResourceError('resource-relative-name-invalid');error.substage=substage
   with self.assertRaises(P.RuntimeInspectionError) as caught:
    with P.runtime_stage('resource-inventory'):raise error
   self.assertEqual(caught.exception.diagnostic.get('resource_substage'),'catalog-alias-inputs' if substage=='catalog-alias-inputs' else None)
   self.assertNotIn('private',json.dumps(caught.exception.diagnostic))
 def test_unknown_stage_rejected(self):
  with self.assertRaises(P.EvidenceError):
   with P.runtime_stage('/private/stage'):self.fail('unreachable')
if __name__=='__main__':unittest.main()
