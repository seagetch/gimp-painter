from pathlib import Path
import json,hashlib,os,shutil,subprocess
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');W=Path('/workspace/shared/current-source-gates-d0dd12c943');S=W/'source';m=json.loads((W/'source-manifest.json').read_text());sha=lambda b:hashlib.sha256(b).hexdigest();shutil.copy2(W/'source-manifest.json',W/'initial-symlink-source-manifest.json');shutil.copy2(W/'adapter.log',W/'initial-symlink-adapter-rejection.log');hard=linked=0
for p,r in m['files'].items():
 f=S/p
 if not f.exists():f.symlink_to(Path('/tmp/current-source-gates-9abddbd319/source')/p)
 assert sha(f.read_bytes())==r['sha256'],p
 if p.startswith('app/')and f.is_symlink():
  src=f.resolve();assert not src.is_symlink();data=src.read_bytes();f.unlink();f.write_bytes(data);r['linked']=False;r['materialized_from_verified_blob']=True;hard+=1
 elif not p.startswith('app/')and not f.is_symlink():
  src=R/p
  if src.is_file()and sha(src.read_bytes())==r['sha256']:f.unlink();f.symlink_to(src);r['linked']=True;linked+=1
(W/'source-manifest.json').write_text(json.dumps(m,indent=2)+'\n');(W/'materialization.json').write_text(json.dumps({'app_materialized_verified_blobs':hard,'verified_nonapp_links':linked,'same_git_blobs_and_sha256':True},indent=2)+'\n')
for name,cmd in [('adapter',['tools/check_painter_legacy_adapters.py','--root',str(S),'--report',str(W/'adapter.json')]),('persistence',['migration/tests/test_persistence_field_contract.py','--root',str(S),'--report',str(W/'persistence.json')])]:
 with(W/(name+'.log')).open('w')as f:r=subprocess.run(['python3',*cmd],cwd=S,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'},stdout=f,stderr=subprocess.STDOUT)
 (W/(name+'.status')).write_text(str(r.returncode)+'\n');print(name,r.returncode,flush=True)
changed=[p for p,r in m['files'].items()if sha((S/p).read_bytes())!=r['sha256']];(W/'source-after.json').write_text(json.dumps({'commit':m['commit'],'checked':len(m['files']),'changed':changed},indent=2)+'\n');assert not changed;print('unchanged',len(m['files']),flush=True)
