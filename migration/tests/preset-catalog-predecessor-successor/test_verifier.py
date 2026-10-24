#!/usr/bin/env python3
"""Pinned-evidence controls, with one hash-updated semantic receipt mutation."""
import hashlib,importlib.util,json,pathlib,tempfile
from unittest.mock import patch
ROOT=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('catalog_predecessor_gate',E/'verify.py');gate=importlib.util.module_from_spec(spec);spec.loader.exec_module(gate)
record=gate.verify(ROOT);rejected=0
with tempfile.TemporaryDirectory() as tmp:
    root=pathlib.Path(tmp);files=set(record['history_sha256'])|set(record['current_source_sha256'])|set(record['catalog_receipts'])|{gate.PACKAGE+'/transition.json',gate.PACKAGE+'/catalog-sources.tar.gz'}
    for name in files:
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.symlink_to(ROOT/name)
    for name in [*record['current_source_sha256'],next(iter(record['history_sha256'])),*record['catalog_receipts']]:
        p=root/name;b=p.read_bytes();p.unlink();p.write_bytes(b+b' ')
        try:gate.verify(root)
        except AssertionError:rejected+=1
        else:raise AssertionError(('mutation accepted',name))
        p.unlink();p.symlink_to(ROOT/name)
    name='migration/tests/preset-catalog/normal-results.json';p=root/name;rows=json.loads(p.read_text());rows[0]['returncode']=1;p.unlink();p.write_text(json.dumps(rows))
    forged=dict(record);forged['catalog_receipts']=dict(record['catalog_receipts']);forged['catalog_receipts'][name]=hashlib.sha256(p.read_bytes()).hexdigest()
    q=root/gate.PACKAGE/'transition.json';q.unlink();q.write_text(json.dumps(forged));pin=hashlib.sha256(q.read_bytes()).hexdigest()
    with patch.object(gate,'PIN',pin):
        try:gate.verify(root)
        except AssertionError:rejected+=1
        else:raise AssertionError('hash-updated failed runtime accepted')
assert rejected==19
print('PASS:19 source/history/receipt controls, including hash-updated failed runtime')
