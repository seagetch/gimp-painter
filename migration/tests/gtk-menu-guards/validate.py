#!/usr/bin/env python3
"""Validate receipts independently; never reinterpret failed runs as successful."""
import hashlib,json,pathlib,struct
P=pathlib.Path(__file__).resolve().parent;R=P.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(p=P):
 r=json.loads((p/'report.json').read_text());lock=json.loads((R/'migration/patches/gtk-3.24.49-menu-guards/source-lock.json').read_text())
 assert r['baseline_strict_exit']==1 and r['fixed_strict_exit']==0
 assert 'FINAL dialogs=2 gdk=1 gtk=1 other=0 passed=0' in (p/'source-baseline-strict/stdout.log').read_text()
 assert 'FINAL dialogs=2 gdk=0 gtk=0 other=0 passed=1' in (p/'fixed/stdout.log').read_text()
 assert (p/'fixed/stderr.log').read_text()==''
 for phrase in ['offscreen grab remains rejected','mapped normal grab acquire/release','unrealized menu null-window grab acquire/release']:assert 'PASS '+phrase in (p/'grab-controls.log').read_text()
 assert (p/'atspi-server.log').read_text()=='MENU 1\nPOINTER 1\nPASS external AT-SPI menu and native pointer\n'
 assert '1080x1920x24, authenticated accepted, empty authority rejected' in (p/'controls-run.log').read_text()
 assert 'DBUS_COOKIE_SHA1' in (p/'dbus-loopback.conf').read_text() and 'ANONYMOUS' not in (p/'dbus-loopback.conf').read_text()
 assert 'NO_AT_BRIDGE=0' in (p/'run-atspi.sh').read_text() and 'G_DEBUG=fatal-warnings' in (p/'run-atspi.sh').read_text()
 for n,h in lock['patch_inputs'].items():assert sha(p/'patched-source'/n)==h['after_sha256']
 assert sha(R/'migration/patches/gtk-3.24.49-menu-guards/0001-safe-menu-grab-accessible-ancestors.patch')==lock['patch_sha256']
 runtime=json.loads((p/'runtime-sha256.json').read_text());assert runtime['lib/x86_64-linux-gnu/libgtk-3.so.0.2417.32']==r['runtime_gtk_sha256']
 assert runtime['share/doc/gtk-menu-guards/COPYING']==sha(p/'license-COPYING')
 assert runtime['share/doc/gtk-menu-guards/debian-copyright']==sha(p/'license-debian-copyright')
 rows=json.loads((p/'gimp-integration/results.json').read_text());assert len(rows)==1 and rows[0]['result']=='OK' and rows[0]['returncode']==0 and rows[0]['duration']<60
 assert 'ok 1 /painter-tool/layer-mask/visual-capture' in rows[0]['stdout']
 assert '/gtk-menu-guards/prefix/lib/x86_64-linux-gnu/libgtk-3.so.0.2417.32' in (p/'gimp-integration/loaded-gtk-maps.txt').read_text()
 assert (p/'gimp-integration/executable-before.sha256').read_bytes()==(p/'gimp-integration/executable-after.sha256').read_bytes()
 for name in ['mask-applied.png','mask-disabled.png']:assert struct.unpack('>II',(p/'gimp-integration'/name).read_bytes()[16:24])==(1080,1920)
 assert r['build_script_completed'] and r['reproduction_source_verified']
 assert (p/'reproduced/strict-exit-code.txt').read_text().strip()=='0'
 assert 'FINAL dialogs=2 gdk=0 gtk=0 other=0 passed=1' in (p/'reproduced/strict-stdout.log').read_text()
 assert (p/'reproduced/atspi-server.log').read_text()==(p/'atspi-server.log').read_text()
 assert (p/'reproduced/grab-controls.log').read_text()==(p/'grab-controls.log').read_text()
 assert json.loads((p/'reproduced-runtime-sha256.json').read_text())['lib/x86_64-linux-gnu/libgtk-3.so.0.2417.32']==r['reproduced_runtime_gtk_sha256']
 assert r['gimp_oncanvas'] and r['gimp_capture_inspected'] and not r['release_bundle_rebuilt'] and not r['global_dependency_root_modified']
if __name__=='__main__':
 check();m=json.loads((P/'evidence-sha256.json').read_text())
 for name,h in m.items():assert sha(P/name)==h,name
 print('PASS: source-locked GTK guards; causal red/fixed zero;3 grabs;external AT-SPI+pointer;actual oncanvas portrait;source/license/runtime hashes')
