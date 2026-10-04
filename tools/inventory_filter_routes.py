#!/usr/bin/env python3
"""Inventory old FilterLayer eligibility and explicit modern route dispositions.

Extract public procedure metadata only. Never archive pluginrc/profile files or
inherited environment. The old popup policy is pinned by source hash. Current
GEGL operations are semantic counterparts to audit, not executable mappings.
"""
import argparse, csv, hashlib, io, json, re, subprocess, tarfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--legacy', type=Path, default=ROOT.parent/'gimp-painter-legacy')
p.add_argument('--registry', type=Path, required=True)
p.add_argument('--output', type=Path, default=ROOT/'migration/inventory')
a = p.parse_args()
PIN = 'afa43fae3e920210146abed514f136fd49f671b5'
assert subprocess.check_output(['git','-C',str(a.legacy),'rev-parse','HEAD'],text=True).strip() == PIN

def forms(text, keyword):
    # GIMP caches can contain binary icon strings. Latin1 is a lossless scan;
    # only ASCII registration names/path/type identifiers leave this parser.
    for match in re.finditer(r'\('+re.escape(keyword)+r'\s+',text):
        depth=0; quoted=False; escape=False
        for end in range(match.start(),len(text)):
            c=text[end]
            if quoted:
                if escape: escape=False
                elif c=='\\': escape=True
                elif c=='"': quoted=False
            elif c=='"': quoted=True
            elif c=='(': depth+=1
            elif c==')':
                depth-=1
                if depth==0:
                    yield text[match.start():end+1]; break
        else: raise ValueError('Unterminated registration form')

def allowed(path):
    return path.startswith(('<Image>/Filters/','<Image>/Colors/')) and not path.startswith(tuple(
        '<Image>/Filters/'+x for x in ('Animation','Render','Language','Web','Alpha to Logo','Combine','Decor')))

# Read immutable pinned blobs: the legacy oracle worktree may carry capture
# instrumentation, which must never change source provenance or line numbers.
with tarfile.open(fileobj=io.BytesIO(subprocess.check_output(
        ['git','-C',str(a.legacy),'archive',PIN,'plug-ins','app','data/layer-presets']))) as archive:
    old_bytes={member.name:archive.extractfile(member).read()
               for member in archive if member.isfile()}
old_files={path:data.decode(errors='replace') for path,data in sorted(old_bytes.items())
           if path.startswith('plug-ins/') and Path(path).suffix in ('.c','.h','.scm')}
new_files={str(f.relative_to(ROOT)):f.read_text(errors='replace') for f in sorted((ROOT/'plug-ins').rglob('*'))
           if f.suffix in ('.c','.h','.scm','.py') and not any(x in f.parts for x in ('build','build-debian13'))}
actions=(ROOT/'app/actions/filters-actions.c').read_text()
operations=set(re.findall(r'"((?:gegl|gimp):[a-z0-9-]+)',actions))
# These are explicit source-audited thematic counterparts. Similar labels do
# not establish byte, alpha, radius, parameter, context or output compatibility.
counterparts={
'alienmap2':'alien-map','apply-canvas':'texturize-canvas','applylens':'apply-lens',
'autostretch-hsv':'stretch-contrast-hsv','blur':'box-blur','c-astretch':'stretch-contrast',
'colorify':None,'colors-channel-mixer':'channel-mixer','colortoalpha':'color-to-alpha',
'convmatrix':'convolution-matrix','deinterlace':'deinterlace','dilate':'value-propagate',
'dog':'difference-of-gaussians','edge':'edge','erode':'value-propagate','exchange':'color-exchange',
'flarefx':'lens-flare','gauss':'gaussian-blur','glasstile':'tile-glass','gradmap':'gradient-map',
'hsv-noise':'noise-hsv','laplace':'edge-laplace','make-seamless':'tile-seamless',
'mblur':'motion-blur-linear','neon':'edge-neon','nlfilt':None,'nova':'supernova',
'palettemap':'map-palette','papertile':'tile-paper','pixelize':'pixelize',
'polar-coords':'polar-coordinates','randomize-hurl':'noise-hurl','randomize-pick':'noise-pick',
'randomize-slur':'noise-slur','rgb-noise':'noise-rgb','rotate-colormap':'color-rotate',
'sel-gauss':'gaussian-blur-selective','small-tiles':'tile','sobel':'edge-sobel',
'spread':'noise-spread','vinvert':'value-invert','vpropagate':'value-propagate',
'sharpen':'unsharp-mask','threshold-alpha':'gimp:threshold-alpha'}
nonpixel={'borderaverage','ccanalyze','colormap-remap','compose','decompose-registered','recompose',
          'smooth-palette','tile','pagecurl','curve-bend','map-object'}
context={'filter-pack','gflare','gimpressionist','gradmap','hot','iwarp','lic','lighting',
         'palettemap','rotate-colormap','sample-colorize','warp','bump-map','displace'}
exact={'plug-in-edge','plug-in-gauss','plug-in-gauss-iir','plug-in-gauss-rle','plug-in-gauss-iir2','plug-in-gauss-rle2'}
new_exact={'plug-in-vinvert','plug-in-max-rgb','plug-in-threshold-alpha'}
rows=[]
def row(name,menu,args,source_kind,source_paths):
    short=name.removeprefix('plug-in-')
    native=[path for path,text in new_files.items() if '"'+name+'"' in text and not path.endswith('plug-in-compat.scm')]
    op=counterparts.get(short,short)
    if op and ':' not in op: op='gegl:'+op
    if op not in operations: op=''
    if name in exact: route='exact independent byte kernel; native-double extension'; gap='precision/extension validation'
    elif name in new_exact: route='audited point kernel in development'; gap='genuine byte and native precision validation'
    elif name == 'plug-in-blinds':
        route='audited isolated bundled PDB adapter; U8 nonlinear RGB/Gray; angle 0..90, segments 1..100'
        gap='selection/component phase integration, owner progress forwarding, disabled-swap route, relocation and platform verification'
    elif name == 'plug-in-small-tiles':
        route='audited isolated bundled PDB adapter; U8 nonlinear RGB/Gray; factor 0..6; shared owner context'
        gap='owner progress forwarding, disabled-swap alternative, arbitrary-size/global-latency and other-platform verification'
    elif name == 'plug-in-retinex':
        route='audited isolated bundled PDB adapter; U8 nonlinear RGB; native RGB/RGBA BPP and old float cvar; shared owner context'
        gap='Gray/higher precision, owner progress forwarding, disabled-swap alternative, arbitrary-size/global-latency and other-platform verification'
    elif short in nonpixel: route='isolated legacy image/output adapter required'; gap='multi-image/layer/palette/result semantics, not a pixel substitution'
    elif short in context or source_kind=='script-source': route='isolated context/resource adapter required'; gap='capture resource/context/secondary-object dependencies; no automatic script execution'
    else: route='explicit compatibility kernel or isolated plug-in adapter required'; gap='parameter/alpha/numerical parity and cancellation/crash isolation'
    rows.append(dict(procedure=name,eligibility=source_kind,menu=';'.join(menu),argument_types=','.join(map(str,args)),
        legacy_source=';'.join(source_paths),current_source_mentions=';'.join(native),current_gegl_counterpart=op,
        executor_route=route,missing_port_work=gap,
        dependency_status=('bounded Retinex route implemented; see migration/contracts/filter-retinex.md' if name == 'plug-in-retinex' else
            'bounded SmallTiles route implemented; see migration/contracts/filter-small-tiles.md' if name == 'plug-in-small-tiles' else
            'bounded route implemented; see migration/contracts/filter-process-bridge.md' if name == 'plug-in-blinds' else
            'bundled original source exists; missing port code' if source_paths else 'registration-only; source/dependency unresolved')))
registry=a.registry.read_text(encoding='latin1')
for form in forms(registry,'proc-def'):
    name=re.match(r'\(proc-def\s+"([^"]+)"',form)[1]
    paths=re.findall(r'\(menu-path "([^"]+)"',form)
    args=[int(n) for n in re.findall(r'\(proc-arg (\d+)',form)]
    if 13 in args and 16 in args and any(map(allowed,paths)):
        matches=[p for p,s in old_files.items() if p.endswith(('.c','.h')) and '"'+name+'"' in s]
        row(name,paths,args,'installed-compiled-plugin',matches)
compiled=len(rows)
for path,text in old_files.items():
    if not path.endswith('.scm'): continue
    for form in forms(text,'script-fu-register'):
        match=re.match(r'\(script-fu-register\s+"([^"]+)"',form)
        if not match: continue # abstract helper/dynamic registration is an explicit inventory limit
        name=match[1]
        paths=[]
        for menu in forms(text,'script-fu-menu-register'):
            strings=re.findall(r'"([^"]+)"',menu)
            if strings[0]==name:paths+=strings[1:]
        if 'SF-IMAGE' in form and 'SF-DRAWABLE' in form and any(map(allowed,paths)):
            row(name,paths,re.findall(r'\bSF-[A-Z-]+\b',form),'script-source',[path])
for name in sorted(exact|new_exact):
    if not any(r['procedure']==name for r in rows):
        f=next(f for f in forms(registry,'proc-def') if re.match(r'\(proc-def\s+"([^"]+)"',f)[1]==name)
        row(name,re.findall(r'\(menu-path "([^"]+)"',f),[int(n) for n in re.findall(r'\(proc-arg (\d+)',f)],
            'direct-definition-alias-or-non-popup',[p for p,s in old_files.items() if p.endswith(('.c','.h')) and '"'+name+'"' in s])
a.output.mkdir(parents=True,exist_ok=True)
with (a.output/'filter-procedure-routes.tsv').open('w') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]),delimiter='\t',lineterminator='\n');writer.writeheader();writer.writerows(sorted(rows,key=lambda r:r['procedure']))
callers=[]
for path,data in sorted(old_bytes.items()):
    if not path.startswith('app/') or Path(path).suffix not in ('.c','.cpp','.h','.hpp'):continue
    for i,line in enumerate(data.decode(errors='replace').splitlines(),1):
        if re.search(r'(set_procedure|gimp_filter_layer_set_procedure|PROP_FILTER_SPEC|create_filter_list)',line):
            callers.append(dict(source=path,line=i,code=line.strip()))
for path,data in sorted(old_bytes.items()):
    if not path.startswith('data/layer-presets/') or not path.endswith('.json'):continue
    for i,line in enumerate(data.decode().splitlines(),1):
        if 'plug-in-' in line: callers.append(dict(source=path,line=i,code=line.strip()))
(a.output/'filter-definition-callers.json').write_text(json.dumps(callers,indent=2)+'\n')
sources={path for row in rows for path in row['legacy_source'].split(';') if path}
sources|={'app/widgets/gimplayerpopup.cpp','app/core/gimpfilterlayer.cpp','app/pdb/pdb-cxx-utils.hpp'}
report=dict(schema_version=1,legacy_commit=PIN,installed_compiled_popup_procedures=compiled,
    source_eligible_scripts=sum(r['eligibility']=='script-source' for r in rows),total_rows=len(rows),
    limitations=['Source Script-Fu eligibility is not proof of runtime registration or safe pixel-only semantics.',
      'Current source/name and GEGL counterpart are discovery evidence, not numerical compatibility or automatic execution.',
      'Optional external third-party plug-ins are unbounded; unavailable definitions remain preserved and visibly unsupported.',
      'Registry input is not archived; only procedure metadata sourced from the pinned public project is emitted.'],
    source_sha256={name:hashlib.sha256(old_bytes[name]).hexdigest() for name in sorted(sources)},
    current_actions_sha256=hashlib.sha256((ROOT/'app/actions/filters-actions.c').read_bytes()).hexdigest())
(a.output/'filter-procedure-inventory.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ('installed_compiled_popup_procedures','source_eligible_scripts','total_rows')}))
