#!/usr/bin/env python3
import pathlib,json,hashlib,sys
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
for p,h in json.loads((P/'artifacts.json').read_text()).items():assert sha((P/p).read_bytes())==h,p
sys.path.insert(0,str(R/'migration/tests'));import persistence_unsafe_undo_successor as gate
r=gate.reviewed(R);s=json.loads((P/'source-overlay.json').read_text());assert sha((R/'migration/tests/persistence_unsafe_undo_successor.py').read_bytes())==s['module_sha256'];assert s['baseline_files_verified']==s['baseline_after_unchanged']==16474 and s['no_native_execution']and s['held_worktree_excluded']
j=json.loads((P/'metadata-report.json').read_text());assert j['result']=='PASS'and(j['counts']['contract_records'],j['counts']['syntax_candidates'],j['counts']['routes'])==(1379,873,16)
assert 'Ran 7 tests'in(P/'controls.log').read_text()and '\nOK\n'in(P/'controls.log').read_text()
d=json.loads((P/'manifest-diff.json').read_text());assert len(d['source_changes'])==len(d['added_successor'])==1 and d['all_families_routes_crossdomains_preserved'];assert d['source_changes'][0][0]['path']==gate.PATH
print('PASS exact image source/header successor;1379/873/16 preserved;7 scoped controls; immutable runtime receipt')
