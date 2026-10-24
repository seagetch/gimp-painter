#!/usr/bin/env python3
"""Validate bounded diagnostic receipts, not product lifecycle completion."""
import hashlib,json,pathlib
P=pathlib.Path(__file__).resolve().parent
for name,digest in json.loads((P/'artifacts.json').read_text()).items():
 assert hashlib.sha256((P/name).read_bytes()).hexdigest()==digest,name
for name,code in [('baseline',0),('naive',250),('timestamp-older',0),('timestamp-equal',250)]:
 r=json.loads((P/(name+'.json')).read_text());assert r['returncode']==code,name
 assert r['runtime_inputs_unchanged'],name
 if code:assert "gdk_display_device_is_grabbed(gdkdisplay,device)' should be TRUE" in r['stdout']+r['stderr'],name
 else:assert 'ok 1 /grab-gap/newer-window' in r['stdout'],name
s=json.loads((P/'diagnostic-scope.json').read_text());assert s['diagnostic_only'] is True
assert s['files']['app/display/gimpdisplayshell-grab.c']['clean']==s['files']['app/display/gimpdisplayshell-grab.c']['main']
assert hashlib.sha256((P/'test-grab-gap-v1.c').read_bytes()).hexdigest()==s['fixture_sha256']
assert json.loads((P/'baseline55-build.json').read_text())['unchanged'] is True
readme=(P/'README.md').read_text();assert 'remains unchanged/open' in readme and 'No sanitizer' in readme
print('PASS: bounded newer-grab diagnostic and timestamp controls; original cleanup remains open')
