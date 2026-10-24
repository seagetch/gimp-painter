#!/usr/bin/env python3
import pathlib,json,hashlib,re,struct,argparse
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text());sha=lambda b:hashlib.sha256(b).hexdigest()
for n,h in read('artifact-sha256.json').items():assert sha((E/n).read_bytes())==h,n
x=read('native-result.json');assert x['returncode']==0 and len(re.findall(r'^ok \d+',x['stdout'],re.M))==1;assert 'paired screenshot directory 'in x['stdout'];rect=re.findall(r'rendered image rectangle [^:]+: (\d+,\d+ \d+x\d+)',x['stdout']);assert len(rect)==4 and len(set(rect))==1
assert '1080x1920x24' in (E/'native.log').read_text();assert len(read('held-source-seal.json'))==9
for n in ['00-baseline.png','01-painted.png','02-undo.png','03-redo.png']:
 b=(E/'screenshots'/n).read_bytes();assert b[:8]==b'\x89PNG\r\n\x1a\n'and struct.unpack('>II',b[16:24])==(1040,1800)
x=read('source.json');assert sha((E/'tested-fixture.cpp').read_bytes())==x['sha256'];s=(E/'tested-fixture.cpp').read_text();assert 'g_dir_make_tmp' in s and 'gimp_painter_canvas_ui_get_visible' in s and 'visible_after!=visible_before' in s and '==visible_before' in s and '==visible_after' in s
if a.check_source:assert sha((R/x['path']).read_bytes())==x['sha256']
print('PASS plain-Meson default capture: actual portrait/oncanvas UI1, four asserted native screenshots')
