#!/usr/bin/env python3
"""Negative tests for independently closable legacy-delta action ledgers."""
import copy
import sys
import unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from audit_legacy_granularity import specification,validate

class AssignmentGates(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.expected,_=specification();cls.rows=cls.expected[:3]
    def test_fresh_assignments_do_not_claim_implementation(self):
        self.assertTrue(all(r['status']=='TODO' for r in self.expected))
        self.assertEqual(validate(self.rows,copy.deepcopy(self.rows)),[])
    def test_missing_and_duplicate_rows_fail(self):
        self.assertTrue(any('missing' in e for e in validate(self.rows,self.rows[:-1])))
        self.assertTrue(any('duplicate' in e for e in validate(self.rows,self.rows+[self.rows[0]])))
    def test_section_level_or_multi_action_replacement_fails(self):
        for value in ('04','04.002,04.008','unassigned'):
            actual=copy.deepcopy(self.rows);actual[0]['wbs_task']=value
            self.assertTrue(any('wbs_task' in e for e in validate(self.rows,actual)))
    def test_blob_or_range_or_acceptance_drift_fails(self):
        for field in ('source_blob','source_range','source_sha256','acceptance'):
            actual=copy.deepcopy(self.rows);actual[0][field]='changed'
            self.assertTrue(any(field in e for e in validate(self.rows,actual)))
    def test_done_without_evidence_fails(self):
        actual=copy.deepcopy(self.rows);actual[0]['status']='DONE'
        self.assertTrue(any('DONE requires' in e for e in validate(self.rows,actual)))
    def test_release_cannot_accept_assignment_only(self):
        self.assertTrue(any('unfinished TODO' in e for e in validate(self.rows,self.rows,True)))
    def test_complete_evidence_only_closes_that_item(self):
        actual=copy.deepcopy(self.rows)
        actual[0].update(status='DONE',owner='test',artifact='test artifact',test='test command',result='pass')
        self.assertEqual(validate(self.rows,actual),[])
        self.assertEqual(len([e for e in validate(self.rows,actual,True) if 'unfinished' in e]),2)
if __name__=='__main__':unittest.main(verbosity=2)
