"""Export NEW measurements; does not validate an old seal. Python standard library."""
from pathlib import Path
import hashlib,json,shutil,subprocess
b=Path(__file__).resolve().parent;dest=b/'deliverables/migration/fixtures/legacy-filter-timeline';dest.mkdir(parents=True,exist_ok=True)
subprocess.run(['python3',str(b/'collect-provenance.py')],check=True)
for name in ['capture-filter-timeline.c','instrument.py','build.sh','run.sh','collect-provenance.py','package.py','run_authenticated_display.py','README.md','summarize.py','REPRODUCE.md','source-overlay.patch','patch-check.log','build-provenance.json','dependencies.txt','edge-dependencies.txt','runtime-byte-inventory.json','binary.sha256','core-build.log','build.log']:
 shutil.copy2(b/name,dest/name)
shutil.copy2(b/'gimp/app/core/legacy-timeline.h',dest/'legacy-timeline.h')
for root in ['captures','repeat-captures']:
 for p in (b/root).rglob('*'):
  if p.is_file() and p.name in ['capture.log','exit-code','original-before.sha256','original-after.sha256','results.json','authenticated-client.log','unauthenticated-client.log','runner.log','server.log']:
   d=dest/p.relative_to(b);d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d)
for root in ['headless-captures','headless-repeat-captures']:
 for p in (b/root).rglob('*'):
  if p.is_file() and p.name in ['capture.log','exit-code','original-before.sha256','original-after.sha256']:
   d=dest/'headless-diagnostics'/p.relative_to(b);d.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,d)
records={} 
for root in ['captures','repeat-captures']:
 for scene in ['async','stack','cancel','latecancel','detach','builtin']:
  log=(dest/root/scene/'capture.log').read_text();events=[];states=[]
  for line in log.splitlines():
   if line.startswith(('TL ','STATE ')):
    row=dict(part.split('=',1) for part in line.split()[1:]);row['time_us']=int(row['time_us']);(events if line.startswith('TL ') else states).append(row)
  records[root+'/'+scene]={'events':events,'states':states,'exit_code':int((dest/root/scene/'exit-code').read_text())}
(dest/'timeline.json').write_text(json.dumps(records,indent=2)+'\n')
subprocess.run(['python3',str(b/'summarize.py'),str(dest/'timeline.json'),str(dest/'MEASUREMENTS.md')],check=True)
files={str(p.relative_to(dest)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(dest.rglob('*')) if p.is_file() and p.name!='manifest.json'}
(dest/'manifest.json').write_text(json.dumps({'task':'02.013','legacy_commit':'afa43fae3e920210146abed514f136fd49f671b5','files':files},indent=2)+'\n')
print(dest)
