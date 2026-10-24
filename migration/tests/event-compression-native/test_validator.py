from pathlib import Path
import tempfile,shutil,subprocess,json,hashlib,os
E=Path(__file__).resolve().parent;R=E.parents[2];sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
for case in ['failed-runtime-resealed','missing-sanitizer-resealed','hide-known-skip-resealed','provider-drift']:
 with tempfile.TemporaryDirectory() as d:
  d=Path(d);P=d/'package';shutil.copytree(E,P);root=d/'repo';scope=json.loads((E/'scope.json').read_text())
  for n in {**scope['providers'],**scope['fixture']}:
   target=root/n;target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/n,target)
  env=dict(os.environ,GIMP_EVENT_PROOF_REPO=str(root))
  r=subprocess.run(['python3',str(P/'validate.py')],env=env,capture_output=True,text=True);assert r.returncode==0,r.stderr
  if case=='provider-drift':
   p=root/next(iter(scope['providers']));p.write_text(p.read_text()+'\n/* drift */\n')
  else:
   name='final-sanitizer-build.json' if case=='missing-sanitizer-resealed' else 'final-normal.json';p=P/name;j=json.loads(p.read_text())
   if case=='failed-runtime-resealed':j['returncode']=1
   if case=='missing-sanitizer-resealed':j['compile'].remove('-fsanitize=address,undefined')
   if case=='hide-known-skip-resealed':j['stdout']=j['stdout'].replace(' # SKIP by request (-s option)','')
   p.write_text(json.dumps(j));seal=json.loads((P/'artifact-sha256.json').read_text());seal[name]=sha(p);(P/'artifact-sha256.json').write_text(json.dumps(seal))
  r=subprocess.run(['python3',str(P/'validate.py')],env=env,capture_output=True,text=True);assert r.returncode!=0,case
print('PASS four native event validator rejection controls; copied baseline passes first')
