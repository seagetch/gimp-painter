#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Refresh only Surface in the recorded private native precision build.

Hold the shared build lock. Retain the predecessor executable/report and its
archived source inputs; use new private Surface object, archive aliases and exe.
"""
import argparse, hashlib, json, pathlib, subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--predecessor',type=pathlib.Path,required=True);p.add_argument('--report',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];build=a.build.resolve();r=json.loads(a.predecessor.read_text())
name='app/paint/painter-mypaint-surface/gegl-surface.cpp'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert not r['changed_during_build'];assert sha(pathlib.Path(r['executable']))==r['executable_sha256']
for n,h in r['sources_sha256'].items():
    if n.endswith(('.h','.hpp')):assert sha(root/n)==h,n
commands=[c for c in r['commands'] if '-c' in c and (build/c[c.index('-c')+1]).resolve()==root/name]
assert len(commands)==1
command=list(commands[0]);old_object=command[command.index('-o')+1]
output=build/'mypaint-precision-background-sanitizers';output.mkdir(exist_ok=True)
command[command.index('-o')+1]=str(output/'gegl-surface.cpp.o')
link=list(r['commands'][-1]);exe=build/'app/tests/painter-mypaint-precision-background-asan';link[link.index('-o')+1]=str(exe)
report={'scope':'Post-review background setter only; all other private instrumented/RTTI objects retain the predecessor source snapshot', 'predecessor':str(a.predecessor), 'predecessor_sha256':sha(a.predecessor), 'retained_source_snapshot':'migration/tests/mypaint-precision-sanitizer-source-snapshot.tar.gz', 'refreshed_sources_sha256':{name:sha(root/name)}, 'retained_objects_sha256':{}, 'commands':[], 'sanitizers':r['sanitizers'], 'leak_detection':False}
for c in r['commands']:
    if '-c' in c:
        obj=pathlib.Path(c[c.index('-o')+1]);report['retained_objects_sha256'][str(obj)]=sha(obj)
subprocess.run(command,cwd=build,check=True);report['commands'].append(command)
for archive in sorted(set(x for x in link if x.endswith('.a'))):
    members=subprocess.check_output(['ar','t',archive],cwd=build,text=True).splitlines()
    if old_object not in members:continue
    new=output/pathlib.Path(archive).name
    assert not new.exists(),new
    cmd=['ar','crsT',str(new)]+[str(output/'gegl-surface.cpp.o') if x==old_object else str((build/x).resolve()) for x in members]
    subprocess.run(cmd,cwd=build,check=True);report['commands'].append(cmd)
    link=[str(new) if x==archive else x for x in link]
subprocess.run(link,cwd=build,check=True);report['commands'].append(link)
assert report['refreshed_sources_sha256'][name]==sha(root/name)
for n,h in report['retained_objects_sha256'].items():assert sha(pathlib.Path(n))==h,n
report.update(executable=str(exe),executable_sha256=sha(exe),changed_during_build=[],runner_sha256=sha(pathlib.Path(__file__)))
a.report.write_text(json.dumps(report,indent=2)+'\n');print(report['executable_sha256'])
