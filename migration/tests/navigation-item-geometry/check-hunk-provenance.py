#!/usr/bin/env python3
"""Verify original196 full payloads; this is not behavioral task completion."""
import hashlib,json,pathlib,sys
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
sys.path.insert(0,str(R/'tools'));import assign_legacy_hunks as legacy
original=json.loads((E/'audit.json').read_text());full=json.loads((E/'full-hunk-audit.json').read_text())
assert len(original['rows'])==len(full['rows'])==196
assert original['task']=='25.009' and original['criterion']=='canvas と overlay が同じ角度を使用'
assert len({r['work_id'] for r in original['rows']})==196
sections=legacy.patch_sections(full['base_revision'],full['source_revision'])
index={r['child_id']:r for r in legacy.read_tsv(R/'migration/inventory/changed-hunks.tsv')}
for a,b in zip(original['rows'],full['rows']):
 assert all(a[k]==b[k] for k in ['work_id','source_id','path'])
 h=index[a['source_id']];section=sections[(a['path'],h['index'])]
 payload=(E/b['payload']).read_bytes()
 assert payload==section['payload']
 assert hashlib.sha256(payload).hexdigest()==a['source_sha256']==b['sha256']
 assert a['source_range']==b['range']==section['header']
 assert b['removed_lines']==len(section['removed']) and b['added_lines']==len(section['added'])
 assert a['original_acceptance']==original['criterion']
print(json.dumps(dict(status='PASS',full_payloads=196,removed_lines=sum(r['removed_lines'] for r in full['rows']),behavioral_completion=False)))
