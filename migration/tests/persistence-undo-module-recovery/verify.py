#!/usr/bin/env python3
"""Read-only missing-module recovery receipt, not whole persistence PASS."""
import hashlib,json,pathlib,tarfile
root=pathlib.Path(__file__).resolve().parents[3];p=pathlib.Path(__file__).resolve().parent
for n,h in json.loads((p/'artifacts.json').read_text()).items():assert hashlib.sha256((p/n).read_bytes()).hexdigest()==h,n
assert hashlib.sha256((root/'migration/tests/persistence_undo_successor.py').read_bytes()).hexdigest()=='a6ca50421576d5144413559fe3dbdad50f08d2bd745f9c181404bcaa0b18a76c'
with tarfile.open(p/'source-gate-receipts.tar.gz') as t:
 read=lambda n:t.extractfile(n).read().decode()
 assert "ModuleNotFoundError: No module named 'persistence_undo_successor'" in read('persistence.log')
 assert 'Ran 3 tests' in read('recovered-controls.log') and '\nOK\n' in read('recovered-controls.log')
 assert "('unreviewed persistence source drift', 'native', 'app/paint/gimppaintersmudge.cpp')" in read('recovered-persistence.log')
 seal=json.loads(read('recovered-source-after.json'));assert seal['changed']==[] and seal['checked_committed_inputs']==16378
 assert seal['base_commit']=='9abddbd31938730acaf5d79caf18d2222a4e848e'
 assert 'PASS: 2533 sources; 720 reviewed data sites; 4 dormant legacy findings' in read('adapter.log')
print('PASS: omitted module restored;3 focused groups pass; separate whole-persistence drift remains FAIL')
