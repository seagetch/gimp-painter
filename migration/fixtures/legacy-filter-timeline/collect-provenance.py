from pathlib import Path
import hashlib,json,subprocess,tempfile
b=Path(__file__).resolve().parent;g=b/'gimp';h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
changed=subprocess.check_output(['git','diff','--name-only','HEAD'],cwd=g,text=True).splitlines();patch=subprocess.check_output(['git','diff','--binary','HEAD'],cwd=g);(b/'source-overlay.patch').write_bytes(patch)
with tempfile.TemporaryDirectory(dir=b) as tmp:
 root=Path(tmp)
 for name in changed:
  p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(subprocess.check_output(['git','show','HEAD:'+name],cwd=g))
 r=subprocess.run(['git','apply','--check',str(b/'source-overlay.patch')],cwd=root,capture_output=True,text=True);(b/'patch-check.log').write_text('exit='+str(r.returncode)+'\n'+r.stdout+r.stderr);r.check_returncode()
files=changed+['app/core/legacy-timeline.h','app/core/libappcore.a','app/tests/.libs/test-xcf','app/xcf/libappxcf.a','app/paint/libapppaint.a','app/base/libappbase.a']
(b/'build-provenance.json').write_text(json.dumps({'legacy_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=g,text=True).strip(),'sha256':{p:h(g/p) for p in files},'inherited_build':'legacy-filter-positive-20261015/gimp, copied without hardlinks; core and test executable rebuilt; before/after seals cover only installed bin/gimp-2.8; no prefix-wide historical byte seal','dependencies':h(b.parent/'legacy-filter-positive-20261015/build/environment.json')},indent=2)+'\n')
r=subprocess.run(['bash','-c','source "$1/../legacy-filter-positive-20261015/build/env.sh"; ldd "$1/gimp/app/tests/.libs/test-xcf"','provenance',str(b)],capture_output=True,text=True);(b/'dependencies.txt').write_text(r.stdout+r.stderr);r.check_returncode()
# Postcapture byte inventory: not a retroactive before/after prefix seal.
import datetime,re
captured=datetime.datetime.now(datetime.timezone.utc).isoformat()
edge=b.parent/'legacy-filter-positive-20261015/build/prefix/lib/gimp/2.0/plug-ins/edge'
r=subprocess.run(['bash','-c','source "$1/../legacy-filter-positive-20261015/build/env.sh"; ldd "$2"','provenance',str(b),str(edge)],capture_output=True,text=True);(b/'edge-dependencies.txt').write_text(r.stdout+r.stderr);r.check_returncode()
loader_logs={'test-xcf':(b/'dependencies.txt').read_text(),'edge':r.stdout+r.stderr}
assert all('not found' not in log for log in loader_logs.values())
resolved={}
for executable,log in loader_logs.items():
 for path in re.findall(r'(?:=>\s+|^\s*)(/[^\s]+)',log,re.M):
  p=Path(path)
  resolved.setdefault(str(p),{'resolved_path':str(p.resolve()),'sha256':h(p),'size':p.stat().st_size,'loader_roots':[]})['loader_roots'].append(executable)
(b/'runtime-byte-inventory.json').write_text(json.dumps({'captured_executor_utc':captured,'scope':'Postcapture current edge executable and loader-resolved dependencies of test-xcf and edge. These are not historical process maps, not all dynamically loaded modules, and not before/after runtime-library hashes. No claim that the entire prefix was byte-unchanged during capture.','edge':{'path':str(edge),'sha256':h(edge),'size':edge.stat().st_size},'dependencies':resolved},indent=2)+'\n')
