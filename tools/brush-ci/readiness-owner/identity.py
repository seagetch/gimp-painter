#!/usr/bin/env python3
"""Separate diagnostic overlay/build identity; primary receipt is only a precondition."""
import argparse
import hashlib
import math
import time
import importlib.util
import os
from pathlib import Path
import shlex
import shutil
import stat
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import preframe_identity as original
import matched_provenance as inventory
spec=importlib.util.spec_from_file_location('owner_generate',Path(__file__).with_name('generate.py'))
gen=importlib.util.module_from_spec(spec);spec.loader.exec_module(gen)
MESON='app/tests/meson.build'

def budget_valid(value):
    original.require(isinstance(value,dict) and set(value)=={'started_monotonic','step_seconds','controller_seconds','cleanup_seconds','outer_margin_seconds'})
    original.require(type(value['started_monotonic']) in (int,float) and math.isfinite(value['started_monotonic']) and value['started_monotonic']>0)
    original.require(value['step_seconds']==1200 and value['controller_seconds']==180 and value['cleanup_seconds']==9 and value['outer_margin_seconds']==30)
    return value

def observation_admitted(value, now):
    b=budget_valid(value)
    return b['started_monotonic']<=now<=b['started_monotonic']+1200-180-9-30


def overlay(source, primary):
    original.require(original.command(['git','-C',str(source),'rev-parse','HEAD']).strip()==primary['github_sha'])
    original.require(original.command(['git','-C',str(source),'diff','--name-only','HEAD']).splitlines()==[MESON])
    original.require(original.command(['git','-C',str(source),'ls-files','--others','--exclude-standard'])=='')
    base=original.command(['git','-C',str(source),'show','HEAD:'+MESON])
    append=(source/'tools/brush-ci/readiness-owner/meson.append').read_text()
    original.require((source/MESON).read_text()==base+append)
    original.require(original.digest(Path(primary['source'])/MESON)==hashlib.sha256(base.encode()).hexdigest())
    submodule=original.command(['git','-C',str(source),'rev-parse','HEAD:gimp-data']).strip()
    original.require(original.command(['git','-C',str(source/'gimp-data'),'rev-parse','HEAD']).strip()==submodule)
    original.require(original.command(['git','-C',str(source/'gimp-data'),'status','--porcelain=v1','--untracked-files=all'])=='')
    return {'base_tree':primary['source_tree'],'modified_path':MESON,'overlay_sha256':original.digest(source/MESON),'append_sha256':original.digest(source/'tools/brush-ci/readiness-owner/meson.append'),'gimp_data':submodule}

def snapshot(original_receipt):
    j=original.verify(original_receipt)
    original.require(j['original_exit']==1)
    root=original.cleanpath(str(Path(j['run'])/'readiness-owner-private'))
    original.require(root.stat().st_uid==os.getuid() and stat.S_IMODE(root.stat().st_mode)==0o700)
    source,build=root/'source',root/'build'
    for p in (source,build):original.cleanpath(str(p));original.require(p.is_dir())
    original.require(source!=Path(j['source']) and build!=Path(j['build']))
    ov=overlay(source,j)
    budget=budget_valid(original.readjson(root/'budget.json'))
    targets=original.readjson(build/'meson-info/intro-targets.json',limit=32*1024*1024)
    target=[t for t in targets if t.get('name')=='readiness-owner' and t.get('type')=='executable']
    binary=build/'app/tests/readiness-owner'
    original.require(len(target)==1 and target[0]['filename']==[str(binary)] and os.access(binary,os.X_OK))
    options=original.readjson(build/'meson-info/intro-buildoptions.json')
    san=[x['value'] for x in options if x['name']=='b_sanitize'];original.require(len(san)==1)
    original.require(set(san[0] if isinstance(san[0],list) else san[0].split(','))=={'address','undefined'})
    tests=original.readjson(build/'meson-info/intro-tests.json');tests=[x for x in tests if x['name']=='readiness-owner']
    original.require(len(tests)==1 and tests[0]['cmd']==[str(binary)] and tests[0]['timeout']==180)
    directory=source/'tools/brush-ci/readiness-owner'
    for name in ('identity.py','generate.py'):
        original.require(original.digest(Path(__file__).with_name(name))==original.digest(directory/name))
    original.require(original.digest(Path(inventory.__file__).resolve())==original.digest(source/'tools/brush-ci/matched_provenance.py'))
    paths=[binary,Path(original_receipt),source/MESON,build/'compile_commands.json',build/'build.ninja']
    paths += [build/'meson-info'/name for name in ('intro-buildoptions.json','intro-targets.json','intro-tests.json')]
    paths += [directory/name for name in ('generate.py','probe.inc','phase.inc','phase_records.py','leaf.inc','leaf_records.py','image.inc','image_records.py','icon.inc','icon_records.py','collect.py','identity.py','run.sh','meson.append')]
    paths += [source/'tools/brush-ci'/name for name in ('preframe_identity.py','sample_case14.py','matched_provenance.py')]
    paths += [Path(j['run'])/'deps/env.sh',source/'migration/baseline/debian13-package-lock.json',root/'locale.json',root/'budget.json']
    with tempfile.TemporaryDirectory(prefix='owner-identity-',dir=root) as temp:
        gen.generate(source,temp)
        for name in ('readiness-owner-main.c','readiness-owner-transform.inc','readiness-owner-probe.inc','readiness-owner-brush.inc','readiness-owner-phase.inc','readiness-owner-leaf.inc','readiness-owner-image.inc','readiness-owner-icon.inc'):
            generated=build/'app/tests'/name
            original.require(original.digest(generated)==original.digest(Path(temp)/name));paths.append(generated)
    commands=original.readjson(build/'compile_commands.json',limit=32*1024*1024)
    for row in commands:
        original.require(Path(row['directory'])==build)
        args=row.get('arguments') or shlex.split(row['command']);tool=shutil.which(args[0]);original.require(tool is not None)
        paths.append(Path(tool).resolve())
    for name in ('meson','ninja'):
        tool=shutil.which(name);original.require(tool is not None);paths.append(Path(tool).resolve())
    # Separate fresh Meson artifacts; not an ad-hoc replay or a primary binary.
    paths += [p for p in build.rglob('*') if p.is_file() and not p.is_symlink() and (p.suffix in ('.o','.a','.so') or '.so.' in p.name)]
    lock=original.readjson(source/'migration/baseline/debian13-package-lock.json');packages={}
    deps=Path(j['run'])/'deps'
    for p in lock['packages']:
        original.require(original.digest(deps/'apt/archives'/p['filename'])==p['sha256']);packages[p['filename']]=p['sha256']
    external={};dep_hash=inventory.tree_hash(deps/'root',packages,external)
    locale_hash=inventory.tree_hash(root/'locale')
    return {'format':1,'acceptance_eligible':False,'diagnostic_only':True,
        'original_receipt':str(original.cleanpath(original_receipt)),'github_sha':j['github_sha'],
        'source_tree':j['source_tree'],'budget':budget,'overlay':ov,'source':str(source),'build':str(build),
        'binary':str(binary),'primary_identity_unchanged':True,'separate_build':True,
        'dependency_root_sha256':dep_hash,'dependency_archives':packages,'dependency_external_files':external,
        'locale_sha256':locale_hash,'host_scope':'selected public dependency links only; not full host closure',
        'hashes':{str(p):original.digest(p) for p in sorted(set(paths))}}

def verify(receipt):
    j=original.readjson(original.cleanpath(receipt),limit=32*1024*1024)
    original.require(j==snapshot(j['original_receipt']));return j

def main():
    parser=argparse.ArgumentParser();sub=parser.add_subparsers(dest='mode',required=True)
    for mode in ('capture','verify','summary','wrapper'):
        c=sub.add_parser(mode);c.add_argument('--receipt',required=True);c.add_argument('--output',required=True)
        if mode=='wrapper':c.add_argument('args',nargs=argparse.REMAINDER)
    a=parser.parse_args()
    try:
        j=snapshot(a.receipt) if a.mode=='capture' else verify(a.receipt)
        if a.mode=='wrapper':
            original.require(a.args==['--',j['binary'],'-p',original.CASE])
            original.require(os.environ.get('ASAN_OPTIONS')==original.ASAN and os.environ.get('UBSAN_OPTIONS')==original.UBSAN and 'LSAN_OPTIONS' not in os.environ)
            controller=Path(j['source'])/'tools/brush-ci/readiness-owner/collect.py'
            original.require(original.digest(controller)==j['hashes'][str(controller)])
            if not observation_admitted(j['budget'],time.monotonic()):
                original.write(a.output,{'format':1,'acceptance_eligible':False,'status':'inconclusive-budget-insufficient','controller_started':False})
                return 2
            os.execv(sys.executable,[sys.executable,'-B',str(controller),'--binary',j['binary'],'--sha256',j['hashes'][j['binary']],'--output',a.output])
        if a.mode=='summary':
            j={k:j[k] for k in ('format','acceptance_eligible','diagnostic_only','github_sha','source_tree','primary_identity_unchanged','separate_build','dependency_root_sha256','locale_sha256','host_scope','overlay')}|{'status':'diagnostic-identity-verified','binary_sha256':j['hashes'][j['binary']],'identity_sha256':inventory.hashed(j),'hashed_file_count':len(j['hashes'])}
        original.write(a.output,j);return 0
    except (original.Invalid,OSError,ValueError,TypeError,KeyError,original.subprocess.SubprocessError,inventory.EvidenceError):
        try:original.write(a.output,{'format':1,'acceptance_eligible':False,'status':'inconclusive-identity-invalid'})
        except (original.Invalid,OSError):pass
        return 2
if __name__=='__main__':sys.exit(main())
