#!/usr/bin/env python3
"""Hash-updated causal evidence negatives, not application reruns."""
import pathlib,json,hashlib,tempfile,subprocess,shutil
E=pathlib.Path(__file__).resolve().parent
cases=[('normal-results.json',lambda j:j[0].update(stdout=j[0]['stdout'].replace('ok 1 ','notok 1 ',1))),('sanitizer-results.json',lambda j:j[0].update(returncode=1)),('mutants.json',lambda j:j[0].update(returncode=0)),('held-provider-exclusion.json',lambda j:j['paths'][0].update(sha256='0'*64)),('regression-results.json',lambda j:j[0].update(result='FAIL'))]
for name,edit in cases:
 with tempfile.TemporaryDirectory(prefix='catalog-proof-control-') as temp:
  root=pathlib.Path(temp)
  for p in E.iterdir():
   if p.is_file():(root/p.name).symlink_to(p)
  data=json.loads((E/name).read_text());edit(data);(root/name).unlink();(root/name).write_text(json.dumps(data))
  m=json.loads((E/'manifest.json').read_text());m['artifacts'][name]=hashlib.sha256((root/name).read_bytes()).hexdigest();(root/'manifest.json').unlink();(root/'manifest.json').write_text(json.dumps(m))
  result=subprocess.run(['python3',str(E/'verify.py'),'--root',str(root)],capture_output=True,text=True)
  assert result.returncode!=0,(name,result.stdout);assert 'AssertionError' in result.stderr,(name,result.stderr)
  print('PASS rejected hash-updated',name)
