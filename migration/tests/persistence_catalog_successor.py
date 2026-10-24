"""Exact native preset catalog source admission; original field seals unchanged."""
import hashlib
import io
import json
from pathlib import Path
import re
import tarfile
from persistence_source import function, norm
PACKAGE = 'migration/tests/persistence-catalog-successor'
GIMP = 'app/core/gimp.c'
DESCRIPTOR = 'app/presets/gimppresetdescriptor.c'
PINS = {'transition.json': '79bd9148d40909f1328263bd4f3edb4d36f7195d06bac1d527afc37d4c2cd867', 'sources.tar.gz': '692b22dbd23605a3d0768e5c143eb5ee606384bd7152d9b3d9d99fab247970db'}
def sha(data): return hashlib.sha256(data).hexdigest()

def semantic_proof(before, after):
    for name in ('gimp_class_init','gimp_set_property','gimp_get_property','gimp_load_config'):
        assert norm(function(before[GIMP],name)) == norm(function(after[GIMP],name)), ('Gimp persistence changed',name)
    # Added instance-private lifecycle is not a new property/schema/backend.
    assert 'G_DEFINE_TYPE_WITH_PRIVATE (Gimp, gimp, GIMP_TYPE_OBJECT)' in after[GIMP]
    assert 'GimpPresetOwner preset_owner;' in after[GIMP]
    descriptor=after[DESCRIPTOR]
    assert 'GIMP_CONFIG_PATH_DIR_LIST' in descriptor
    assert 'G_PARAM_READWRITE' in descriptor
    assert 'g_object_class_find_property' in descriptor
    assert 'gimp_layer_preset_load' in descriptor
    assert not re.search(r'GIMP_CONFIG_PROP_[A-Z_]+\s*\(',descriptor)

def reviewed(root):
    root=Path(root);blobs={n:(root/PACKAGE/n).read_bytes() for n in PINS}
    for n,b in blobs.items():assert sha(b)==PINS[n], ('catalog proof changed',n)
    record=json.loads(blobs['transition.json']);rows=record['providers']
    assert record['schema_version']==1 and len(rows)==len({r['path'] for r in rows})==4
    assert re.fullmatch(r'[0-9a-f]{40}',record['successor_commit'])
    with tarfile.open(fileobj=io.BytesIO(blobs['sources.tar.gz']),mode='r:gz') as t:
        assert all(m.isfile() for m in t.getmembers())
        assert len(t.getnames())==len(set(t.getnames()))==5
        archived={m.name:t.extractfile(m).read() for m in t.getmembers()}
    assert set(archived)=={'predecessor/'+GIMP}|{'successor/'+r['path'] for r in rows}
    before={GIMP:archived['predecessor/'+GIMP].decode()};after={}
    for row in rows:
        p=row['path'];b=archived['successor/'+p]
        assert sha(b)==row['successor_sha256']
        if p==GIMP:assert sha(archived['predecessor/'+p])==row['predecessor_sha256']
        assert (root/p).read_bytes()==b, ('unreviewed catalog source drift',p)
        after[p]=b.decode()
    semantic_proof(before,after)
    proof={p:(root/p).read_bytes() for p in record['proof']}
    for p,b in proof.items():assert sha(b)==record['proof'][p], ('catalog runtime receipt changed',p)
    prefix='migration/tests/preset-catalog/'
    read=lambda name:json.loads(proof[prefix+name])
    manifest=read('source-manifest.json')
    with tarfile.open(fileobj=io.BytesIO(proof[prefix+'tested-inputs.tar.gz']),mode='r:gz') as t:
        for row in rows:
            p=row['path'];assert manifest['owned_sha256'][p]==row['successor_sha256']
            assert t.extractfile(p).read()==archived['successor/'+p]
    counts={'preset-catalog':26,'preset-catalog-ui':5,'preset-gui-entries':6}
    normal=read('normal-results.json');san=read('sanitizer-results.json')
    assert len(normal)==len(san)==3
    for r in normal:
        assert r['returncode']==0 and r['result']=='OK'
        assert len(re.findall(r'^ok \d+',r['stdout'],re.M))==counts[r['name'].split(' / ')[-1]]
    for r in san:
        assert r['returncode']==0
        assert len(re.findall(r'^ok \d+',r['stdout'],re.M))==counts[r['target']]
        assert not re.search(r'ERROR: AddressSanitizer|runtime error:',r['stdout']+(r['stderr'] or ''))
    build=read('sanitizer-build.json');assert len(build['instrumented_sources'])==11
    assert not build['whole_application_instrumented'] and not build['leak_detection']
    for p in build['instrumented_sources']:assert manifest['input_sha256'][p]==build['sources_sha256'][p]
    assert read('sanitizer-source-before.json')==read('sanitizer-source-after.json')
    assert read('link-inputs-before.json')==read('link-inputs-after.json')
    mutants=read('mutants.json');assert len(mutants)==2
    for r in mutants:assert r['returncode']!=0 and r['expected_assertion'] in r['stdout']+r['stderr']
    assert b'autosave' in proof[prefix+'cleanup-red.log'] and b'SIGABRT' in proof[prefix+'cleanup-red.log']
    regressions=read('regression-results.json');assert len(regressions)==6
    assert all(r['result']=='OK' and r['returncode']==0 for r in regressions)
    return record

def resolve_source(root,predecessor,actual):
    record=reviewed(root);row=next(r for r in record['providers'] if r['path']==GIMP)
    assert predecessor==row['predecessor_sha256'], 'wrong catalog predecessor'
    assert actual==row['successor_sha256'], 'wrong catalog successor'
    return dict(revision='native',path=GIMP,predecessor_sha256=predecessor,
                successor_sha256=actual,commits=[record['successor_commit']],
                catalog_semantics=record['semantics'],catalog_runtime_proof=record['proof'])
