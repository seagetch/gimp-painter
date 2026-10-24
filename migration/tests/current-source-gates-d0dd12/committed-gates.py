from pathlib import Path
import subprocess,hashlib,json,os
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');commit=subprocess.check_output(['git','rev-parse','d0dd12c943'],cwd=R,text=True).strip();W=Path('/workspace/shared/current-source-gates-'+commit[:10]);S=W/'source';S.mkdir(parents=True,exist_ok=True);B=Path('/tmp/current-source-gates-9abddbd319/source');M=json.loads(Path('/tmp/current-source-gates-9abddbd319/current-55bfa5f1f7/source-manifest.json').read_text());sha=lambda b:hashlib.sha256(b).hexdigest();records={};linked=copied=0
proc=subprocess.Popen(['git','cat-file','--batch'],cwd=R,stdin=subprocess.PIPE,stdout=subprocess.PIPE)
for entry in subprocess.check_output(['git','ls-tree','-rz',commit],cwd=R).split(b'\0'):
 if not entry:continue
 meta,name=entry.split(b'\t',1);mode,kind,blob=meta.decode().split();name=name.decode()
 if kind!='blob' or not(name.startswith(('app/','libgimp','pdb/','tools/','migration/'))or name in ('tasks.md','meson.build')):continue
 p=S/name;p.parent.mkdir(parents=True,exist_ok=True)
 if p.exists()or p.is_symlink():p.unlink()
 if name in M['files']and M['files'][name]['git_blob']==blob:
  data=(B/name).read_bytes();assert sha(data)==M['files'][name]['sha256'],name;p.symlink_to(B/name);linked+=1
 else:
  proc.stdin.write(blob.encode()+b'\n');proc.stdin.flush();header=proc.stdout.readline().split();data=proc.stdout.read(int(header[2]));assert proc.stdout.read(1)==b'\n';p.write_bytes(data);copied+=1
 assert hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()==blob,name
 records[name]={'git_blob':blob,'sha256':sha(data),'linked':p.is_symlink()}
proc.stdin.close();assert proc.wait()==0
if not(S/'.git').exists():(S/'.git').symlink_to(R/'.git')
(W/'source-manifest.json').write_text(json.dumps({'commit':commit,'scope':'Exact Git-only selected app/library/tools/migration inputs; every file checked against this commit Git blob; no native build','files':records,'copied':copied,'linked':linked},indent=2)+'\n');print(commit,len(records),copied,linked,flush=True)
for name,cmd in [('adapter',['tools/check_painter_legacy_adapters.py','--root',str(S),'--report',str(W/'adapter.json')]),('persistence',['migration/tests/test_persistence_field_contract.py','--root',str(S),'--report',str(W/'persistence.json')])]:
 with(W/(name+'.log')).open('w')as f:r=subprocess.run(['python3',*cmd],cwd=S,env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'},stdout=f,stderr=subprocess.STDOUT)
 (W/(name+'.status')).write_text(str(r.returncode)+'\n');print(name,r.returncode,flush=True)
changed=[p for p,r in records.items()if sha((S/p).read_bytes())!=r['sha256']];(W/'source-after.json').write_text(json.dumps({'commit':commit,'checked':len(records),'changed':changed},indent=2)+'\n');assert not changed;print('unchanged',len(records),flush=True)
