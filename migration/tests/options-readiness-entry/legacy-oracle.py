"""Source-bound old getter transition oracle, not an executable legacy UI proof."""
from pathlib import Path
import subprocess,re,hashlib,json
R=Path('/workspace/scratch/0015fc5d1a03/gimp-painter');O=Path('/workspace/shared/options-readiness-oracle');O.mkdir(exist_ok=True);pin='afa43fae3e920210146abed514f136fd49f671b5'
paths=['app/paint/gimpmypaintoptions.cpp','app/paint/gimpmypaintoptions.h','app/core/gimpcontext.c','app/core/gimptoolinfo.c','app/tools/gimpmypaintbrushoptions-gui.cpp','app/widgets/gimpcontainerview.c']
inputs={}
for p in paths:
 b=subprocess.check_output(['git','show',pin+':'+p],cwd=R);q=O/p;q.parent.mkdir(parents=True,exist_ok=True);q.write_bytes(b);inputs[p]=hashlib.sha256(b).hexdigest()
s=(O/paths[0]).read_text();assert 'options->brush           = NULL;' in s;assert 'options->brush = GIMP_MYPAINT_BRUSH(brush_copied);' in s;assert 'return options->brush ? TRUE: FALSE;' in s
body=lambda name:re.search(r'\n'+re.escape(name)+r'\s*\([^\{]*\)\s*\{(.*?)\n\}',s,re.S).group(1)
assert 'options->brush =' not in body('gimp_mypaint_options_dispose');assert 'options->brush = NULL;' in body('gimp_mypaint_options_finalize');assert body('gimp_mypaint_options_mypaint_brush_changed').index('g_return_if_fail (GIMP_IS_MYPAINT_BRUSH (data))')<body('gimp_mypaint_options_mypaint_brush_changed').index('options->brush = GIMP_MYPAINT_BRUSH(brush_copied)')
ready=False;trace=[]
for event in ['initial','null','successful adoption','null','successful adoption','dispose']:
 if event=='successful adoption':ready=True
 trace.append({'event':event,'ready':ready})
(O/'oracle.json').write_text(json.dumps({'legacy_pin':pin,'scope':'source-derived successful old brush adoption boolean; no old executable/UI run; invalid pointers and failed duplicate undefined behavior excluded','input_sha256':inputs,'trace':trace,'actual_supported_routes':['MyPaint popup container editor on Options context -> container view selection -> context_set_by_type','MyPaint history selection -> context_set_mypaint_brush','context copy and parent inheritance -> real setter'],'native_implicit_seed_difference':'native constructor auto-selects user resource or standard; explicit selection/copy of same resource needs its successful-request event, not active-state alias'},indent=2)+'\n');print('PASS source-derived old adoption predicate oracle')
