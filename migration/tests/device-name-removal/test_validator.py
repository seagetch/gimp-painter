#!/usr/bin/env python3
import pathlib,json,hashlib,shutil,tempfile,subprocess,os
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];results=[]
for name in ['provider-drift','failed-runtime-resealed','sanitizer-scope-resealed','artifact-drift']:
 with tempfile.TemporaryDirectory(prefix='device-proof-control-') as td:
  r=pathlib.Path(td);e=r/'migration/tests/device-name-removal';shutil.copytree(E,e);(r/'.git').symlink_to(R/'.git',target_is_directory=True)
  for p in ['app/widgets/gimpdevicemanager.c','app/tests/test-device-name-identity.c']:
   (r/p).parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(R/p,r/p)
  if name=='provider-drift':
   p=r/'app/widgets/gimpdevicemanager.c';p.write_text(p.read_text().replace('gimp_device_info_get_device (device_info, NULL) == device','TRUE'))
  elif name=='artifact-drift':
   p=e/'manager-after.c';p.write_bytes(p.read_bytes()+b'\n')
  else:
   f='normal.json' if name=='failed-runtime-resealed' else 'sanitizer-build.json';p=e/f;j=json.loads(p.read_text())
   if name=='failed-runtime-resealed':j['returncode']=1
   else:j['compile'][0].remove('-fsanitize=address,undefined')
   p.write_text(json.dumps(j,indent=2)+'\n');h=json.loads((e/'artifact-sha256.json').read_text());h[f]=hashlib.sha256(p.read_bytes()).hexdigest();(e/'artifact-sha256.json').write_text(json.dumps(h,indent=2)+'\n')
  q=subprocess.run(['python3','-B',str(e/'validate.py')],capture_output=True,text=True,env=dict(os.environ,PYTHONDONTWRITEBYTECODE='1'));assert q.returncode!=0 and 'AssertionError' in q.stderr,(name,q.stdout,q.stderr);results.append({'name':name,'returncode':q.returncode,'rejected':True})
print(json.dumps(results,indent=2))
