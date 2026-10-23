#!/usr/bin/env python3
"""Verify unchanged pinned Space PAN source and explicitly bounded adapter."""
import hashlib,json,pathlib,subprocess
E=pathlib.Path(__file__).resolve().parent;R=E.parents[2]
sha=lambda b:hashlib.sha256(b).hexdigest()
def check(e=E):
 m=json.loads((e/'source.json').read_text());assert m['revision']=='afa43fae3e920210146abed514f136fd49f671b5'
 raw=subprocess.check_output(['git','show',m['revision']+':'+m['path']],cwd=R)
 assert raw==(e/'tool-events.c').read_bytes() and sha(raw)==m['sha256']
 excerpt=(e/'space-pressed.c').read_bytes();assert sha(excerpt)==m['excerpt_sha256']
 assert b''.join(raw.splitlines(keepends=True)[m['start_line']-1:m['end_line']])==excerpt
 start=excerpt.index(b'    case GIMP_SPACE_BAR_ACTION_PAN:');end=excerpt.index(b'    case GIMP_SPACE_BAR_ACTION_MOVE:')
 case=excerpt[start:end];assert case==(e/'pan-case.c').read_bytes();oracle=(e/'oracle.inc').read_bytes();assert oracle.count(case)==1
 assert b'gdk_event_get_state' not in case and b'GDK_SHIFT_MASK' not in case and b'GDK_CONTROL_MASK' not in case
 assert case.index(b'gimp_display_shell_device_to_image_coords')<case.index(b'gimp_display_shell_start_scrolling')
 assert b'#define gimp_display_shell_device_to_image_coords legacy_pan_inverse' in oracle
 assert b'#define gimp_display_shell_start_scrolling legacy_pan_begin' in oracle
 assert b'*(c) = *input' in oracle
 return {'status':'PASS','unchanged_pan_branch':True,'source_only':True,'old_runtime':'USER-SKIPPED'}
if __name__=='__main__':print(json.dumps(check(),sort_keys=True))
