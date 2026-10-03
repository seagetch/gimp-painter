#!/usr/bin/env python3
"""Check current public and private SmallTiles PDBs against sealed evidence.

Run with the configured build environment, under the shared native/build lock.
The caller owns locking, build selection and timeouts. This script changes no
source or original evidence and writes only a new report directory/profile.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from check_small_tiles_evidence import verify, DEFAULT_ARCHIVE

ROOT = Path(__file__).resolve().parents[1]

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, default=ROOT/'build-debian13')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--archive', type=Path, default=DEFAULT_ARCHIVE)
    args = parser.parse_args()
    build, out = args.build.resolve(), args.output.resolve()
    if out.exists() and any(out.iterdir()):
        raise ValueError('Current-capture output must be new/empty')
    out.mkdir(parents=True, exist_ok=True)
    executables = [build/'app/gimp-console-3.0', build/'plug-ins/common/tile-small', build/'plug-ins/common/file-png']
    sources = [ROOT/'plug-ins/common/tile-small.c', Path(__file__)]
    before = {str(p):sha(p) for p in executables+sources}
    started = datetime.now(timezone.utc).isoformat()
    with tempfile.TemporaryDirectory(prefix='small-tiles-pdb-') as temporary:
        home = Path(temporary)
        verify(args.archive, home/'evidence')
        evidence = home/'evidence/small-tiles-evidence'
        old = json.loads((evidence/'pdb/index.json').read_text())['cases']
        public_ids = {c['id'] for c in json.loads((evidence/'public-before/index.json').read_text())['cases']}
        plan = dict(cases=old, public_ids=sorted(public_ids), evidence=str(evidence), output=str(out))
        (out/'plan.json').write_text(json.dumps(plan, indent=2)+'\n')
        batch = r'''from pathlib import Path
import hashlib, json
plan=json.loads(Path(PLAN).read_text())
evidence=Path(plan['evidence']); out=Path(plan['output'])
formats={'RGB':"R'G'B' u8",'RGBA':"R'G'B'A u8",'L':"Y' u8",'LA':"Y'A u8"}
results=[]; signatures={}
for hidden in (False,True):
    for case in plan['cases']:
        if not hidden and case['id'] not in plan['public_ids']: continue
        name='plug-in-painter-small-tiles' if hidden else 'plug-in-small-tiles'
        png='input-'+case['mode']+'-%sx%s.png'%(case['width'],case['height'])
        image=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(evidence/'pdb'/png)))
        assert image is not None,case['id']
        layer=image.get_layers()[0]; rect=Gegl.Rectangle.new(0,0,case['width'],case['height']); fmt=formats[case['mode']]
        original=(evidence/'pdb'/case['input']).read_bytes()
        assert bytes(layer.get_buffer().get(rect,1.0,fmt,Gegl.AbyssPolicy.NONE))==original,case['id']
        proc=Gimp.get_pdb().lookup_procedure(name); assert proc is not None,name
        if name not in signatures:
            signatures[name]=[{'name':p.name,'type':p.value_type.name,'minimum':getattr(p,'minimum',None),'maximum':getattr(p,'maximum',None),'default':str(getattr(p,'default_value',None))} for p in proc.get_arguments()]
        config=proc.create_config(); config.set_property('run-mode',Gimp.RunMode.NONINTERACTIVE)
        config.set_property('image',image); config.set_core_object_array('drawables',[layer]); config.set_property('num-tiles',case['parameters'][0])
        result=proc.run(config); assert result.index(0)==Gimp.PDBStatusType.SUCCESS,(case['id'],result.index(0))
        actual=bytes(layer.get_buffer().get(rect,1.0,fmt,Gegl.AbyssPolicy.NONE))
        expected=(evidence/('pdb' if hidden else 'public-before')/case['output']).read_bytes()
        merged=bytearray(actual); channels=case['channels']
        if hidden and case['mode'].endswith('A'):
            for at in range(0,len(merged),channels):
                if not merged[at+channels-1]: merged[at:at+channels-1]=original[at:at+channels-1]
        assert bytes(merged)==expected,(name,case['id'],'old-byte mismatch')
        results.append({'id':case['id'],'procedure':name,'output_sha256':hashlib.sha256(actual).hexdigest(),'old_comparable_sha256':hashlib.sha256(merged).hexdigest()})
        image.delete()
hidden_tiles=next(p for p in signatures['plug-in-painter-small-tiles'] if p['name']=='num-tiles')
assert hidden_tiles['minimum']==0 and hidden_tiles['maximum']==6,hidden_tiles
rejected=[]
case=plan['cases'][0]; png='input-'+case['mode']+'-%sx%s.png'%(case['width'],case['height'])
image=Gimp.file_load(Gimp.RunMode.NONINTERACTIVE,Gio.File.new_for_path(str(evidence/'pdb'/png)))
layer=image.get_layers()[0]; proc=Gimp.get_pdb().lookup_procedure('plug-in-painter-small-tiles')
for mode in (Gimp.RunMode.INTERACTIVE,Gimp.RunMode.WITH_LAST_VALS):
    config=proc.create_config(); config.set_property('run-mode',mode); config.set_property('image',image)
    config.set_core_object_array('drawables',[layer]); config.set_property('num-tiles',3)
    result=proc.run(config); assert result.index(0)==Gimp.PDBStatusType.CALLING_ERROR,(mode,result.index(0))
    rejected.append(str(mode))
image.delete()
expected=json.loads((evidence/'public-before/signatures.json').read_text())
assert signatures['plug-in-small-tiles']==expected['plug-in-small-tiles'],'public argument metadata changed'
assert len(results)==220,len(results)
(out/'observations.json').write_text(json.dumps({'cases':results,'signatures':signatures,'public_count':24,'hidden_count':196,'public_signature_unchanged':True,'hidden_rejected_run_modes':rejected,'hidden_rejected_count':2},indent=2)+'\n')
print('SMALL_TILES_CURRENT_PASSED=220',flush=True)
'''.replace('PLAN', repr(str(out/'plan.json')))
        (out/'capture.py').write_text(batch)
        for name in ('profile','plug-ins','empty','temp','cache','config','data'):
            (home/name).mkdir(mode=0o700)
        for binary in executables[1:]:
            directory=home/'plug-ins'/binary.name; directory.mkdir(); (directory/binary.name).symlink_to(binary)
        directory=home/'plug-ins/python-eval'; directory.mkdir()
        (directory/'python-eval.py').symlink_to(build/'plug-ins/python/python-eval/python-eval.py')
        (home/'system-gimprc').write_text('# Isolated current SmallTiles verification.\n')
        props={'plug-in-path':'plug-ins','module-path':'empty','interpreter-path':'empty','environ-path':'empty','temp-path':'temp','swap-path':'temp'}
        (home/'profile/gimprc').write_text(''.join('(%s %s)\n'%(k,json.dumps(str(home/v))) for k,v in props.items()))
        tests=json.loads((build/'meson-info/intro-tests.json').read_text())
        configured=next(t['env'] for t in tests if t['name']=='script-fu-startup')
        env=dict(os.environ)
        for key,value in configured.items():
            env[key]=value+os.pathsep+env.get(key,'') if key in ('LD_LIBRARY_PATH','GI_TYPELIB_PATH') else value
        for key in list(env):
            if key.startswith('GIMP_TESTING_') or key in ('GIMP_PLUGIN_DEBUG','GIMP_PLUGIN_DEBUG_WRAP'): del env[key]
        env.update(HOME=str(home),GIMP3_DIRECTORY=str(home/'profile'),XDG_CONFIG_HOME=str(home/'config'),XDG_CACHE_HOME=str(home/'cache'),XDG_DATA_HOME=str(home/'data'))
        command=[str(executables[0]),'-nidfs','--system-gimprc',str(home/'system-gimprc'),'--gimprc',str(home/'profile/gimprc'),'--batch-interpreter=python-fu-eval','-b','exec(open('+repr(str(out/'capture.py'))+').read())','--quit']
        result=subprocess.run(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=300)
        (out/'capture.log').write_bytes(result.stdout)
        registry=(home/'profile/pluginrc').is_file()
    after={str(p):sha(p) for p in executables+sources}
    passed=result.returncode==0 and registry and before==after and b'SMALL_TILES_CURRENT_PASSED=220' in result.stdout
    report=dict(status='passed' if passed else 'failed',started_utc=started,finished_utc=datetime.now(timezone.utc).isoformat(),
                command=command,exit_code=result.returncode,fresh_isolated_registry=registry,
                source_and_executable_sha256=before,stable=before==after,evidence_archive_sha256=sha(args.archive),
                observations_sha256=sha(out/'observations.json') if (out/'observations.json').exists() else None,
                public_cases=24 if passed else None,hidden_cases=196 if passed else None,rejected_run_modes=2 if passed else None,log_sha256=sha(out/'capture.log'))
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report))
    if not passed: print(result.stdout.decode(errors='replace')[-6000:])
    return not passed

if __name__=='__main__':
    raise SystemExit(main())
