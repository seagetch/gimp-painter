import json,hashlib,subprocess
from pathlib import Path
P=Path(__file__).resolve().parent;R=P.parents[2]
h=lambda b:hashlib.sha256(b).hexdigest()
for p,pin in json.loads((P/'artifact-sha256.json').read_text()).items():assert h((P/p).read_bytes())==pin,p
checks=json.loads((P/'probes.json').read_text());assert len(checks)==16 and all(x['returncode']==0 for x in checks)
r=json.loads((P/'receipt.json').read_text());assert r['probe_count']==449 and len(r['route_negative_controls'])==2
base=json.loads(subprocess.check_output(['git','show',r['base']+':migration/inventory/additional-c-header-routes.json'],cwd=R));candidate=json.loads((P/'candidate.json').read_text());assert candidate=={'format':base['format'],'routes':base['routes']+r['rows']}
for row in r['rows']:assert any(row['path'] in x for x in r['route_negative_controls'])
for path in {x['header'] for x in checks}:assert (P/'source'/path).read_bytes()==subprocess.check_output(['git','show',r['base']+':'+path],cwd=R)
print('PASS: exact two-route extension, 16 focused compilation probes, 2 fail-closed route controls; full aggregate pending')
