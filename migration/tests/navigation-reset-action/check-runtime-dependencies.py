#!/usr/bin/env python3
"""Retrospective read/hash-before-parse proof; no fresh runtime/pre-build claim."""
import hashlib,json,pathlib,re
R=pathlib.Path(__file__).resolve().parents[3];E=pathlib.Path(__file__).resolve().parent
def check(root=R,e=E):
 m=json.loads((e/'retrospective-runtime-dependencies.json').read_text());proof=json.loads((e/'manifest.json').read_text());p=m['source'];source=(root/p).read_text()
 assert hashlib.sha256((root/p).read_bytes()).hexdigest()==proof['source_sha256'][p]
 expected={'migration/fixtures/legacy-rotation-operations/gui-captures/rotation.tsv':('bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a',{'retained_old_starts','retained_ctrl_mid_drag'}),'migration/fixtures/legacy-zoom/zoom.tsv':('5fcf9b0f0c66b707a6a57de8e617c49ed9d32dd21750bf1670d2e46ce711a21e',{'drag_check_retained_zoom'})}
 assert len(m['assets'])==2 and {a['path'] for a in m['assets']}==set(expected)
 read_sites=0
 for a in m['assets']:
  data=(root/a['path']).read_bytes();digest,functions=expected[a['path']]
  assert a['sha256']==digest==hashlib.sha256(data).hexdigest() and len(data)==a['bytes']
  assert set(a['native_read_functions'])==functions
  for function in a['native_read_functions']:
   start=source.index('\n'+function+' (');end=source.find('\nstatic ',start+1);body=source[start:end if end>=0 else len(source)]
   read=body.index('g_file_get_contents (path, &contents, NULL, &error)');hash_at=body.index('g_compute_checksum_for_string (G_CHECKSUM_SHA256, contents, -1)');verify=re.search(r'g_assert_cmpstr\s*\(checksum,\s*==,\s*"'+digest+r'"\)',body);parse=body.index('g_strsplit (contents, "\\n", -1)')
   assert verify and read<hash_at<verify.start()<verify.end()<parse
   for component in pathlib.Path(a['path']).parts:assert '"'+component+'"' in body[:read]
   read_sites+=1
  for consumer in a['registered_consumers']:
   for mode in ['normal','sanitizer']:
    stdout=json.loads((e/(mode+'-original.json')).read_text())['stdout'];assert any(line.startswith('ok ') and consumer in line and '# SKIP' not in line for line in stdout.splitlines())
 return dict(status='PASS',retrospective=True,before_build_asset_seal=False,assets=2,native_hash_before_parse_sites=read_sites,runtime_receipts_modified=False)
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
