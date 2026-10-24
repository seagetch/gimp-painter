"""Pinned successful-adoption readiness successor; no saved schema change."""
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
from persistence_source import function, norm, calls
PACKAGE = 'migration/tests/persistence-readiness-successor'
CONTEXT = 'app/core/gimpcontext.c'
OPTIONS = 'app/paint/painter-mypaint-surface/gimp-painter-options.cpp'
PINS = {'transition.json': 'd40df2c023f4993d7200cdb7beee1aca8088fd2ad6e41d4b9c48d18921d4f9ec', 'sources.tar.gz': '538a0df45a564bb8ad0a35c697d86aab027bee732e560db72895468b0359fdc9'}

def sha(data): return hashlib.sha256(data).hexdigest()

def semantic_proof(before, after):
    for name in ('gimp_context_set_property','gimp_context_get_property',
                 'gimp_context_config_iface_init','gimp_context_serialize',
                 'gimp_context_deserialize','gimp_context_serialize_property',
                 'gimp_context_deserialize_property'):
        assert norm(function(before[CONTEXT],name)) == norm(function(after[CONTEXT],name)), ('context serialization changed',name)
    registration = r'GIMP_CONFIG_PROP_[A-Z_]+|g_object_class_install_property|g_param_spec_[a-z_]+'
    old = [(n,args) for n,args,*_ in calls(function(before[CONTEXT],'gimp_context_class_init'),registration)]
    new = [(n,args) for n,args,*_ in calls(function(after[CONTEXT],'gimp_context_class_init'),registration)]
    assert old == new and len(old) == 28, 'context property registrations changed'
    assert sum(n.startswith('GIMP_CONFIG_PROP_') for n,args in old) == 16
    for name in ('set_property','get_property','gimp_painter_mybrush_options_class_init'):
        assert norm(function(before[OPTIONS],name)) == norm(function(after[OPTIONS],name)), ('Options saved metadata changed',name)
    # The added state is private and transient, not a serialized property.
    assert 'gboolean painter_reaffirming;' in after[CONTEXT]
    assert 'G_ADD_PRIVATE (GimpContext)' in after[CONTEXT]
    assert '"painter-mybrush-reaffirmed"' in after[CONTEXT]
    assert 'G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_HOOKS' in after[CONTEXT]
    getter = function(after[OPTIONS],'gimp_mypaint_options_is_ready')
    assert 'i.legacy_adopted ? TRUE : FALSE' in getter
    assert 'store(options).read<OptionsSlot>' in getter
    assert 'bool legacy_adopted=false,implicit_seed=false,legacy_reaffirm_pending=false;' in after[OPTIONS]
    assert 'if(source&&(!own_seed||i.legacy_reaffirm_pending))i.legacy_adopted=true;' in after[OPTIONS]
    assert 'legacy_adopted' not in function(after[OPTIONS],'set_property')


def reviewed(root):
    root = Path(root)
    blobs = {n:(root/PACKAGE/n).read_bytes() for n in PINS}
    for n,b in blobs.items(): assert sha(b)==PINS[n], ('readiness proof changed',n)
    record=json.loads(blobs['transition.json']);rows=record['providers']
    assert len(rows)==len({r['path'] for r in rows})==4
    with tarfile.open(fileobj=io.BytesIO(blobs['sources.tar.gz']),mode='r:gz') as tar:
        assert len(tar.getnames())==len(set(tar.getnames()))==8
        assert all(m.isfile() for m in tar.getmembers())
        archived={m.name:tar.extractfile(m).read() for m in tar.getmembers()}
    assert set(archived)=={side+'/'+r['path'] for r in rows for side in ('predecessor','successor')}
    before={};after={}
    for row in rows:
        p=row['path']
        for side in ('predecessor','successor'): assert sha(archived[side+'/'+p])==row[side+'_sha256']
        assert (root/p).read_bytes()==archived['successor/'+p], ('unreviewed readiness source drift',p)
        before[p]=archived['predecessor/'+p].decode();after[p]=archived['successor/'+p].decode()
    semantic_proof(before,after)
    proof={p:(root/p).read_bytes() for p in record['proof']}
    for p,b in proof.items():assert sha(b)==record['proof'][p], ('readiness runtime receipt changed',p)
    prefix='migration/tests/options-readiness-entry/'
    def read(p):return json.loads(proof[prefix+p])
    manifest=read('green/source-manifest.json')
    for row in rows: assert manifest['owned_sha256'][row['path']]==row['successor_sha256']
    with tarfile.open(fileobj=io.BytesIO(proof[prefix+'green/tested-inputs.tar.gz']),mode='r:gz') as tar:
        for row in rows:assert tar.extractfile(row['path']).read()==archived['successor/'+row['path']]
    runs={r['name'].split(' / ')[-1]:r for r in read('green/normal.json')}
    for name,n in [('gimp-painter-options',24),('mypaint-options-mode',6),('mypaint-enum-public',2),('mypaint-options-readiness',20)]:
        r=runs[name];assert r['returncode']==0 and r['result']=='OK'
        assert len(re.findall(r'^ok \d+',r['stdout'],re.M))==n
    assert runs['mypaint-enum-library-only']['returncode']==0 and len(runs)==5
    r=read('green/asan-results.json');assert r['returncode']==0
    assert len(re.findall(r'^ok \d+',r['stdout'],re.M))==20
    assert not re.search(r'ERROR: AddressSanitizer|runtime error:',r['stdout']+(r['stderr'] or ''))
    b=read('green/asan-build.json');assert len(b['instrumented_sources'])==5
    for p,h in b['instrumented_sources'].items():assert manifest['input_sha256'][p]==h
    assert read('green/asan-post.json')['dependency_objects_unchanged']
    assert read('green/source-after.json')['all_source_unchanged']
    assert read('green/final-input-seal.json')['all_recorded_inputs_unchanged']
    h=read('green/headers.json');assert (h['status'],h['routes'],h['probes'])==('PASS',42,88)
    reds=[read('red/result.json'),*read('reviewed-red/blanket-red-results.json'),read('pre-adoption-red/result.json')]
    assert len(reds)==5
    for i,r in enumerate(reds):
        assert r['returncode']!=0
        assert ('toggle.calls == 0' if i==3 else 'options_readiness_c_consumer(o) == expected') in r['stdout']
        assert ('(2 == 0)' if i==3 else '(0 == 1)') in r['stdout']
    assert [r['ready'] for r in read('legacy-source/oracle.json')['trace']]==[False,False,True,True,True,True]
    return record


def provider(root,path,predecessor,actual):
    data=reviewed(root);row=next((r for r in data['providers'] if r['path']==path),None)
    assert row and row['predecessor_sha256']==predecessor, ('readiness predecessor mismatch',path)
    assert row['successor_sha256']==actual, ('readiness successor mismatch',path)
    return data,row


def resolve_source(root,path,predecessor,actual,previous=None):
    data,row=provider(root,path,predecessor,actual)
    result=dict(previous or dict(revision='native',path=path,predecessor_sha256=predecessor,commits=[]))
    if previous:result['readiness_intermediate_sha256']=predecessor
    result.update(successor_sha256=actual,commits=result['commits']+[data['successor_commit']],
                  readiness_semantics=data['semantics'],readiness_runtime_proof=data['proof'])
    return result
