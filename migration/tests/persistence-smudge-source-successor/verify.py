#!/usr/bin/env python3
import pathlib,json,hashlib,sys
p=pathlib.Path(__file__).resolve().parent;root=p.parents[2];sys.path.insert(0,str(root/'migration/tests'))
import persistence_smudge_source_successor as gate
for n,h in json.loads((p/'artifacts.json').read_text()).items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
assert hashlib.sha256((root/'migration/tests/persistence_smudge_source_successor.py').read_bytes()).hexdigest()==json.loads((p/'source-overlay.json').read_text())['added_module_pins']['migration/tests/persistence_smudge_source_successor.py']
r=json.loads((p/'metadata-report.json').read_text());assert r['result']=='PASS' and (r['counts']['contract_records'],r['counts']['syntax_candidates'],r['counts']['routes'])==(1379,873,16)
s=json.loads((p/'source-overlay.json').read_text());assert s['base_commit']=='9abddbd31938730acaf5d79caf18d2222a4e848e' and s['checked_committed_inputs']==16378 and s['no_native_execution'] and s['held_worktree_excluded']
assert set(s['explicit_changes'])=={'migration/inventory/persistence-field-contract.json','migration/tests/persistence_successors.py','migration/tests/test_persistence_field_contract.py'}
assert 'Ran 2 tests' in (p/'controls.log').read_text() and '\nOK\n' in (p/'controls.log').read_text()
print('PASS: scoped committed Smudge source admission;1379/873/16 on named frozen overlay; no new runtime claim')
