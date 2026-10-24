#!/usr/bin/env python3
import pathlib,json,hashlib,subprocess
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];sha=lambda b:hashlib.sha256(b).hexdigest()
a=json.loads((E/'artifact-sha256.json').read_text());assert set(a)=={p.name for p in E.iterdir() if p.is_file() and p.name not in ['artifact-sha256.json','validate.py']}
for n,h in a.items():assert sha((E/n).read_bytes())==h,n
before=(E/'manager-before.c').read_text();after=(E/'manager-after.c').read_text();start=before.index('static void\ngimp_device_manager_device_removed (');end=before.index('\nstatic void\n',start+1);part=before[start:end];assert part.count('  if (device_info)')==1
replacement='  /* A superseded same-name exposure can be removed after its replacement.\n   * Do not detach the device which currently owns this persisted profile. */\n  if (device_info &&\n      gimp_device_info_get_device (device_info, NULL) == device)'
assert after==before[:start]+part.replace('  if (device_info)',replacement)+before[end:]
assert (R/'app/widgets/gimpdevicemanager.c').read_text()==after
assert (R/'app/tests/test-device-name-identity.c').read_bytes()==(E/'test-device-name-identity.c').read_bytes()
assert subprocess.check_output(['git','show','503ade3466ddebc60d13d20502ebf0eb0c617f96:app/widgets/gimpdevicemanager.c'],cwd=R)==(E/'manager-before.c').read_bytes()
for mode in ['normal','sanitizer']:
 r=json.loads((E/(mode+'.json')).read_text());assert r['returncode']==0
 assert 'ok 1 /device-name/unique-name-restore' in r['stdout'] and 'ok 2 /device-name/superseded-removal' in r['stdout']
 assert 'ERROR: AddressSanitizer' not in r['stderr'] and 'runtime error:' not in r['stderr']
 assert '1080x1920x24' in r['stdout']
 b=json.loads((E/(mode+'-build.json')).read_text());assert b['unchanged']
 assert b['sources'][str(pathlib.Path('/workspace/shared/device-name-identity/test-device-name-identity.c'))]==sha((E/'test-device-name-identity.c').read_bytes())
 assert [x for x in b['link'] if '--wrap=' in x]==['-Wl,--wrap=gdk_device_get_name']
s=json.loads((E/'sanitizer-build.json').read_text());assert len(s['compile'])==2 and all('-fsanitize=address,undefined' in c for c in s['compile'])
for n in ['expected-red.json','mutant.json']:
 r=json.loads((E/n).read_text());assert r['returncode']!=0 and "gimp_device_info_get_device (info, NULL) == attached" in r['stdout']+r['stderr']
 assert 'ok 1 /device-name/unique-name-restore' in r['stdout']
p=json.loads((E/'provenance.json').read_text());assert not p['unexpected_shared_input_changes'] and p['core_regular_archive_members_verified']==254 and not p['whole_app_sanitizer'] and not p['lsan']
assert json.loads((E/'link-inputs-before.json').read_text())==json.loads((E/'link-inputs-after.json').read_text())
print('PASS bounded device removal: normal2/scoped sanitizer2/original-manager mutant; exact five-line change; original physical/platform requirement remains open')
