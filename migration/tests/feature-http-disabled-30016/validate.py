#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,csv,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text())
command=[sys.executable,str(E.parent/'adapter-removal/features-entry-08018/validate.py')]
if a.check_source:command.append('--check-source')
subprocess.run(command,check=True)
v=read('runtime.json');assert v['returncode']==0 and v['has_original_entry'] and not v['http_symbols'];assert 'libsoup' not in v['direct_dynamic_dependencies'];assert 'Optional Painter HTTP service' not in v['stderr']
text=v['stdout']+'\n'+v['stderr'];events=['INIT: gimp_initialize','INIT: gimp_restore','EXIT: gimp_exit','EXIT: gimp_dispose','EXIT: gimp_finalize'];positions=[text.index(x) for x in events];assert positions==sorted(positions)
b=read('build-audit.json');assert b['options']['painter-http']=='disabled' and not b['http_library_targets_present'] and not b['assertions_disabled'];assert hashlib.sha256((E/'tested-config.h').read_bytes()).hexdigest()==b['config_sha256'];assert '#define HAVE_PAINTER_HTTP' not in (E/'tested-config.h').read_text();assert '[707/707]' in (E/'build.log').read_text()
d=read('original13-duty-audit.json');assert not d['original_task_completed'] and len(d['duties'])==13
original=[row for row in csv.DictReader((R/'migration/inventory/hunk-wbs.tsv').open(),delimiter='\t') if '30.016/feature-entry-point' in row['implementation_tasks'].split(',')]
assert [x['original'] for x in d['duties']]==original
assert all(x['owner'] and x['remaining'] for x in d['duties'])
print('PASS: full HTTP-disabled console707 build/runtime; no HTTP symbols/direct soup; all13 original duties retained, original task open')
