#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Refresh only the GTK test in an existing focused MyPaint private build.

Hold the shared build lock. Instrumented production sources and all recorded
headers must still match. Retained RTTI-only bridge objects refer to their
recorded source snapshot, not any later unrelated working-tree edit. This only
builds; the lock-aware native display runner must verify the refreshed binary.
"""
import argparse,gzip,hashlib,json,pathlib,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path,required=True);a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[2];build=a.build.resolve();path=a.report.resolve();raw=path.read_bytes();r=json.loads(raw)
name='app/tests/test-painter-mypaint-tool.cpp'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert not r['changed_during_build']
assert sha(pathlib.Path(r['executable']))==r['executable_sha256']
for n,h in r['sources_sha256'].items():
    if n==name or n in r['rtti_compatibility_only_sources']:continue
    assert sha(root/n)==h,n
commands=[c for c in r['commands'] if '-c' in c and (build/c[c.index('-c')+1]).resolve()==root/name]
assert len(commands)==1
command=commands[0];link=r['commands'][-1]
assert pathlib.Path(command[command.index('-o')+1]).is_relative_to(build/'mypaint-tool-sanitizers')
assert pathlib.Path(link[link.index('-o')+1])==build/'app/tests/painter-mypaint-tool-asan'
predecessor=r.get('test_only_refresh',{}).get('predecessor',path.stem+'-before-test-refresh.json.gz')
if 'test_only_refresh' not in r:(path.parent/predecessor).write_bytes(gzip.compress(raw,mtime=0))
subprocess.run(command,cwd=build,check=True);subprocess.run(link,cwd=build,check=True)
r['test_only_refresh']={'source':name,'predecessor':predecessor,'commands':[command,link],
    'reason':'HALT/COMMIT synthesizes release using elapsed wall time; verify final pixels including any tail. Instrumented production and headers are unchanged. Retained RTTI bridge objects use the archived source snapshot.',
    'runner_sha256':sha(pathlib.Path(__file__))}
r['sources_sha256'][name]=sha(root/name);r['executable_sha256']=sha(pathlib.Path(r['executable']));r['run_status']='Refreshed test only; native GTK run required'
path.write_text(json.dumps(r,indent=2)+'\n');print(r['executable_sha256'])
