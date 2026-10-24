#!/usr/bin/env python3
"""Immutable isolated HOME discovery receipts; no native execution on replay."""
import argparse
import copy
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import tarfile
P=Path(__file__).resolve().parent

def sha(b):return hashlib.sha256(b).hexdigest()

def validate(red,green,source,provider):
    assert sha(source['before.c'])==provider['before_sha256']
    assert sha(source['after.c'])==provider['after_sha256']
    marker=b'  /* The original Painter 2.8 build used this distinct home directory.'
    after=source['after.c'];start=after.index(marker);end=after.index(b'  return install;',start)
    assert after[:start]+after[end:]==source['before.c'], 'unrelated installer change'
    block=after[start:end]
    for token in [b'if (! install->old_dir && g_get_home_dir ())',b'".gimp-painter-2.8"',b'G_FILE_TEST_IS_DIR',b'install->old_major = 2',b'install->old_minor = 8',b'install->migrate   = TRUE']:
        assert token in block, ('fallback semantic token missing',token)
    assert b'painter_origin' not in block
    for side in ('before','after'):
        assert source[side+'-with-helper.c']==source[side+'.c']+source['helper.c']
    for name,r in [('red',red),('green',green)]:
        assert r['source_sha256']['before-install']==provider['before_sha256']
        assert r['source_sha256']['test-helper']==sha(source['helper.c'])
        assert r['source_sha256']['test.cpp']==sha(source[('red' if name=='red' else 'green')+'-test.cpp'])
        assert r['source_sha256']['app/core/gimppainterprofile.cpp']==sha(source['gimppainterprofile.cpp'])
        assert r['clean_source_changed']==r['linked_inputs_changed']==[]
        assert r['clean_source_count']==18814
        assert len(r['linked_inputs_sha256'])==1322 and len(r['runtime_dependency_sha256'])==(125 if name=='red' else 127)
    assert red['linked_inputs_sha256']==green['linked_inputs_sha256']
    red_runs=[r for r in red['results'] if r['name'].endswith('-run')]
    assert len(red_runs)==1 and red_runs[0]['returncode']!=0
    assert '(selected == expected)' in red_runs[0]['stdout'] and '(NULL ==' in red_runs[0]['stdout']
    cases={'missing','regular-file','lone-painter','standard-precedence','xdg-precedence','explicit-precedence','unicode','migrate-retry','migrate-standard'}
    runs={r['name']:r for r in green['results'] if r['name'].endswith('-run')}
    assert set(runs)=={mode+'-'+case+'-run' for mode in ('normal','asan') for case in cases}
    for name,r in runs.items():
        assert r['returncode']==0 and len(re.findall(r'^ok \d+',r['stdout'],re.M))==1,name
        assert not re.search(r'ERROR: AddressSanitizer|runtime error:',r['stdout']+r['stderr']),name
        assert r['preserved_fixture_sha256'],name
        for path in r['preserved_fixture_sha256']:
            assert '/profile-discovery/final-profile-'+name.removesuffix('-run')+'/' in path
    for mode in ('normal','asan'):
        comp=[r for r in green['results'] if r['name'].startswith(mode+'-compile-')]
        assert len(comp)==3 and all(r['returncode']==0 for r in comp)
        for r in comp:
            assert ('-fsanitize=address,undefined' in r['command'])==(mode=='asan')
    deps=green['compiled_dependency_sha256']
    assert deps['/workspace/shared/profile-discovery/after-with-helper.c']==sha(source['after-with-helper.c'])
    assert red['compiled_dependency_sha256']['/workspace/shared/profile-discovery/before-with-helper.c']==sha(source['before-with-helper.c'])
    return {'result':'PASS','normal':9,'selected_three_unit_asan':9,'causal_red':True,'new_ui_claim':False,'original_profile_suite_rerun':False}

def load():
    with tarfile.open(P/'tested-sources.tar.gz') as t:
        assert all(m.isfile() for m in t.getmembers())
        source={m.name:t.extractfile(m).read() for m in t.getmembers()}
    return json.loads((P/'red-results.json').read_text()),json.loads((P/'green-results.json').read_text()),source,json.loads((P/'provider.json').read_text())

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--commit');a=parser.parse_args()
    artifacts=json.loads((P/'artifacts.json').read_text())
    actual={str(f.relative_to(P)) for f in P.rglob('*') if f.is_file() and '__pycache__' not in f.parts and f.name!='artifacts.json'}
    assert actual==set(artifacts)
    for p,h in artifacts.items():assert sha((P/p).read_bytes())==h,p
    red,green,source,provider=load();result=validate(red,green,source,provider)
    assert sha((P/'legacy-configure.ac').read_bytes())==provider['old_configure_sha256']
    assert b'gimpdir=.gimp-painter-gimp_user_version' in (P/'legacy-configure.ac').read_bytes()
    a_manifest=json.loads(gzip.decompress((P/'red-clean-source-manifest.json.gz').read_bytes()))
    b_manifest=json.loads(gzip.decompress((P/'green-clean-source-manifest.json.gz').read_bytes()))
    assert a_manifest==b_manifest and len(a_manifest['complete_source_sha256'])==18814
    assert sha((P/'caller-app.c').read_bytes())==provider['caller_app_sha256']
    caller=(P/'caller-app.c').read_text()
    assert 'g_file_query_file_type (gimpdir, G_FILE_QUERY_INFO_NONE, NULL) !=' in caller
    assert 'G_FILE_TYPE_DIRECTORY)' in caller
    aliases=json.loads((P/'alias-calls.json').read_text())
    assert aliases['runtime_dependencies_unchanged']
    assert {r['case'] for r in aliases['runs']}=={'same','dotdot','symlink','same-standard'}
    for row in aliases['runs']:
        assert row['returncode']!=0
        assert "'gimp_user_install_run(install,1)' should be TRUE" in row['stdout']+row['stderr']
        assert row['source_before']==row['source_after'] and row['source_before']
    if a.commit:
        body=subprocess.check_output(['git','-C',str(P.parents[2]),'show',a.commit+':'+provider['provider']])
        assert sha(body)==provider['after_sha256'],'committed installer differs'
    print(json.dumps(result,sort_keys=True))

if __name__=='__main__':main()
