from pathlib import Path
import json,hashlib,shutil,subprocess,re,platform,csv
b=Path(__file__).resolve().parent;root=b/'gimp';repo=b.parent/'recovery-ui-20261015/source'
out=b/'deliverables/migration/fixtures/legacy-brush-strokes';out.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
def artifact(p):return {'path':str(p.relative_to(out)),'sha256':sha(p)}
patch=subprocess.check_output(['git','-C',str(root),'diff','--binary','HEAD'])
patch+=subprocess.run(['git','diff','--no-index','--','/dev/null',str(root/'app/tests/stroke-capture.cpp')],stdout=subprocess.PIPE).stdout.replace(str(root).encode()+b'/',b'/')
(out/'capture-source.patch').write_bytes(patch)
for name in ['package.py','build.sh','compile.sh','run.sh','reopen.sh','independent.sh','repeat.sh','compiler-version.txt','dependencies.ldd','dependency-versions.txt','compile.log','paint-build.log','test-build.log']:
 shutil.copyfile(b/name,out/name)
shutil.copyfile(b.parent/'legacy-filter-positive-20261015/build/environment.json',out/'baseline-build-environment.json')
shutil.copyfile(repo/'migration/baseline/legacy/debian13-package-lock.json',out/'debian13-package-lock.json')
for name in ['LICENSE','COPYING','data/mypaint-brushes/deevad/readme.txt']:
 dest=out/('deevad-readme.txt' if name.endswith('readme.txt') else name);shutil.copyfile(root/name,dest)
shutil.copyfile(root/'app/core/mypaintbrush-enum-settings.h',out/'pinned-enum-settings.h')
state_names=[name for name,n in re.findall(r'#define (STATE_\w+) (\d+)',(root/'app/core/mypaintbrush-enum-settings.h').read_text()) if name!='STATE_COUNT'];assert len(state_names)==30
libs=[]
for line in (b/'dependencies.ldd').read_text().splitlines():
 m=re.search(r'=> (/\S+)',line) or re.search(r'^\s*(/\S+)',line)
 if m:
  p=Path(m.group(1));libs.append({'path':str(p),'resolved_path':str(p.resolve()),'sha256':sha(p)})
build_options={'configure': next(line for line in (root/'config.status').read_text().splitlines() if line.startswith('ac_cs_config=')), 'makefile_flags': [line for line in (root/'Makefile').read_text().splitlines() if re.match(r'^(CFLAGS|CXXFLAGS|CPPFLAGS|LDFLAGS|CC|CXX) =',line)]}
write(out/'build-options.json',build_options)
evidence_index={}
for name in ['pen','textured_ink','watercolor_expressive']:
 for p in sorted((b/'captures'/name).iterdir()):
  if p.is_file():evidence_index[name+'/'+p.name]=sha(p)
 for fn in ['capture.log','boundary.jsonl','seeds.jsonl','final.rgba','exit-code']:
  evidence_index[name+'/repeat-'+fn]=sha(b/'repeat-captures'/name/fn)
write(out/'runtime-evidence-index.json',evidence_index)
manifest={'runtime_evidence_index':artifact(out/'runtime-evidence-index.json'),'build_options':artifact(out/'build-options.json'),'source_commit':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),'architecture':platform.machine(),'compiler':(b/'compiler-version.txt').read_text(),'compile_command':artifact(out/'compile.sh'),'build_command':artifact(out/'build.sh'),'baseline_build_environment':artifact(out/'baseline-build-environment.json'),'dependency_lock':artifact(out/'debian13-package-lock.json'),'linked_dependency_hashes':libs,'versions_pkg_config':(b/'dependency-versions.txt').read_text().splitlines(),'capture_binary':{'path':str(root/'app/tests/.libs/test-xcf'),'sha256':sha(root/'app/tests/.libs/test-xcf')},'preserved_original_binary':{'path':str(b.parent/'legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8'),'sha256':sha(b.parent/'legacy-filter-positive-20261015/build/prefix/bin/gimp-2.8')},'adaptations':'Full source patch includes inherited build compatibility adaptations and dormant FilterLayer observation, plus new brush-boundary observer and test harness. Original writer, reader, brush equations and surface rendering remain unchanged. Seed conversion is named before the existing g_rand_set_seed call; no seed value is supplied by the harness. Core uses placement-new on zeroed storage because original constructor leaves options pointer uninitialized.','limitations':['Source-faithful rebuilt reference, not historical binary-identical environment','Capture test executable is distinct from preserved installed original binary','No physical tablet, UI event path, or GIMP 3 replay exercised']}
write(out/'build-manifest.json',manifest)
asset_rows={r['path']:r for r in csv.DictReader((repo/'migration/fixtures/legacy-source/assets.tsv').open(),delimiter='\t')}
summaries=[]
for source in ['classic/pen','classic/textured_ink','deevad/watercolor_expressive']:
 name=source.split('/')[-1];src=b/'captures'/name;dest=out/name;dest.mkdir(exist_ok=True)
 for p in src.iterdir():
  if p.is_file():shutil.copyfile(p,dest/p.name)
 repeat=b/'repeat-captures'/name
 for fn in ['capture.log','boundary.jsonl','seeds.jsonl','final.rgba','exit-code']:
  shutil.copyfile(repeat/fn,dest/('repeat-'+fn))
 assert (src/'exit-code').read_text().strip()=='0'
 assert (src/'independent-exit-code').read_text().strip()=='0'
 assert (src/'final.rgba').read_bytes()==(src/'reopened.rgba').read_bytes()==(repeat/'final.rgba').read_bytes()
 initial=(src/'initial.rgba').read_bytes();assert len(set(tuple(initial[i:i+4]) for i in range(0,len(initial),4)))==2
 assert initial!=(src/'final.rgba').read_bytes()
 events=[json.loads(s) for s in (src/'boundary.jsonl').read_text().splitlines()];seeds=[json.loads(s) for s in (src/'seeds.jsonl').read_text().splitlines()];assert len(events)==len(seeds)==18
 assert [s['sequence'] for s in seeds]==list(range(18))
 snap=events[0]['snapshot'];runtime=re.search(r'CAPTURE_RUNTIME utc=(\S+) glib=(\S+) processors=(\d+)',(src/'capture.log').read_text());assert runtime
 settings={'engine_settings':snap['settings'],'snapshot_position':'immediately before first Brush::stroke_to, after effective options/color/alpha-lock','mapping_count':45,'allocated_engine_setting_count':52,**json.loads((src/'options.json').read_text())};write(dest/'effective-settings.json',settings)
 brushpath='data/mypaint-brushes/'+source+'.myb';shutil.copyfile(root/brushpath,dest/'brush.myb');assert sha(dest/'brush.myb')==asset_rows[brushpath]['sha256']
 previewpath='data/mypaint-brushes/'+source+'_prev.png';shutil.copyfile(root/previewpath,dest/'brush-preview.png');assert sha(dest/'brush-preview.png')==asset_rows[previewpath]['sha256']
 resource={**events[0]['surface'],'brushmark':None,'texture':None,'layer_mask':None,'selection':{'empty':True,'effective_mask':None,'width':96,'height':80,'pixel_format':'Y8','bytes':artifact(dest/'selection.u8')},'sample_source':'current drawable RGBA tiles; identical to initial_pixels before first event, subsequently affected by earlier dabs','brush_resource':{**asset_rows[brushpath],'bytes_artifact':artifact(dest/'brush.myb')},'preview_asset':{'source_path':previewpath,'artifact':artifact(dest/'brush-preview.png'),'role':'Loaded by original brush loader for icon only; not a stroke geometry or sampling input'},'brush_shape':'analytic MyPaint hardness/ellipse dabs; radius, aspect, angle evaluated by original engine per dab','transform_phase':'no UI transform; all coords supplied in drawable pixels; drawable offset 0,0; no external pattern phase or brushmark transform','initial_document':artifact(dest/'initial.xcf'),'pixel_layout':'row-major top-to-bottom, left-to-right; RGBA8 straight alpha, 96*4=384-byte rows, no padding','color_space':'untagged legacy RGB8 drawable numeric channels; no ICC profile attached or color transform requested; no claim of calibrated display color','runtime':{'utc':runtime[1],'glib':runtime[2],'num_processors':int(runtime[3])}}
 assert not any(events[0]['surface'][x] for x in ['brushmark_present','texture_present','non_incremental','alpha_lock'])
 assert set((src/'selection.u8').read_bytes())=={0}
 write(dest/'resources.json',resource)
 state={k:v for k,v in snap.items() if k!='settings'};state['state_names']=state_names;state['rng_seed_uint32']=seeds[0]['actual_seed_uint32']
 origin=events[0]['monotonic_ns'];es=[]
 for i,event in enumerate(events):
  assert len(event['snapshot']['state_f32_bits'])==30
  es.append({k:v for k,v in event.items() if k not in ['snapshot','surface']});es[-1]['monotonic_ns']-=origin
 record={'schema_version':1,'evidence_kind':'legacy-runtime-capture','source_commit':manifest['source_commit'],'capture_boundary':'GimpMypaintCore::stroke_to-before-Brush::stroke_to','provenance':{'operator':'Codex isolated original-source test harness; synthetic engine-boundary inputs','captured_utc':runtime[1],'build_manifest':artifact(out/'build-manifest.json'),'instrumentation_patch':artifact(out/'capture-source.patch'),'capture_log':artifact(dest/'capture.log')},'brush':{'source_path':brushpath,'sha256':sha(dest/'brush.myb'),'effective_settings':artifact(dest/'effective-settings.json')},'surface':{'width':96,'height':80,'pixel_format':'RGBA8','color_space':resource['color_space'],'alpha_convention':'straight','initial_pixels':artifact(dest/'initial.rgba'),'resource_manifest':artifact(dest/'resources.json')},'engine_state':state,'events':es,'results':[{'checkpoint_sequence':17,'pixels':artifact(dest/'final.rgba'),'editable_document':artifact(dest/'final.xcf')}]}
 write(out/(name+'.json'),record)
 summary={'name':name,'record':artifact(out/(name+'.json')),'output_sha256':sha(dest/'final.rgba'),'changed_channel_bytes':sum(a!=z for a,z in zip(initial,(src/'final.rgba').read_bytes())),'events':18,'engine_splits':[i for i,e in enumerate(events) if e['engine_split_after']],'reopen_exact_rgba':True,'repeat_original_execution_exact_rgba':True,'preserved_original_binary_reopen':True};summaries.append(summary)
write(out/'summary.json',{'scope':'three representative original-engine strokes on a two-color canvas; synthetic post-transform inputs, genuine runtime outputs','captures':summaries,'acceptance_gaps':['Physical tablet and original UI event/transform paths unmeasured','No imported event replay adapter, cross-machine determinism, enabled texture/brushmark, nonempty selection, alpha lock, nonincremental or resumed state cases','Original startup/type-registration and loader warnings retained; successful capture/reopen is not a clean-runtime claim']})
print(out)
