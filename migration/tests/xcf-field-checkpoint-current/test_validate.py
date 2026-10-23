#!/usr/bin/env python3
import copy,json,unittest
import validate as v
class RejectionControls(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.normal=json.loads((v.HERE/'normal.json').read_text());cls.asan=json.loads((v.HERE/'sanitizers.json').read_text());cls.fields=(v.ROOT/'migration/inventory/xcf-field-acceptance.tsv').read_text()
 def reject(self,fn):
  n=copy.deepcopy(self.normal);a=copy.deepcopy(self.asan);f=self.fields
  result=fn(n,a,f)
  if isinstance(result,str):f=result
  with self.assertRaises((ValueError,KeyError)):v.validate(n,a,f)
 def test_normal_failed(self):self.reject(lambda n,a,f:n[0].update(result='FAIL'))
 def test_missing_suite(self):self.reject(lambda n,a,f:a['results'].pop('xcf'))
 def test_tap_short(self):self.reject(lambda n,a,f:n[0].update(stdout=n[0]['stdout'].replace('ok 1 ','not ok 1 ',1)))
 def test_budget_changed(self):self.reject(lambda n,a,f:a['results']['painter-xcf-open'].update(timeout_seconds=360))
 def test_slow_omitted(self):self.reject(lambda n,a,f:a['results']['painter-xcf-roundtrip'].update(command=['binary']))
 def test_wrapper_omitted(self):self.reject(lambda n,a,f:a['results']['painter-xcf-open'].update(command=['binary']))
 def test_source_drift(self):self.reject(lambda n,a,f:a.update(changed_during_run=['app/xcf/xcf-load.c']))
 def test_unknown_field_assertion(self):self.reject(lambda n,a,f:f.replace('/painter-xcf-fields/all_owner_attributes','/not-executed/field'))
if __name__=='__main__':unittest.main()
