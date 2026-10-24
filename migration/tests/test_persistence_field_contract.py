#!/usr/bin/env python3
"""Original10.001 source-only completeness gate over the fixed16 routes.

The family tests derive/check populations and metadata from source before any
cross-family view is accepted. Derived counts are reports, never the oracle.
No native/legacy executable, runtime ordering or roundtrip claim is involved.
"""
import argparse
from collections import Counter
import copy
import csv
import hashlib
import io
import json
from pathlib import Path
import re
from persistence_source import source, exact, LEGACY
import test_persistence_xcf_contract as xcf
import test_persistence_config_contract as config
import test_persistence_mypaint_contract as mypaint
from persistence_successors import resolve as resolve_source_successor
from persistence_enum_successor import provenance as enum_successor_provenance

ROOT=Path(__file__).resolve().parents[2]
BASE='ade58551ffc6326b8feecadaa3351ba14cd0aa0a'
MANIFEST='migration/inventory/persistence-field-contract.json'
FIELD_VIEW='migration/inventory/persistence-fields.tsv'
SITE_VIEW='migration/inventory/persistence-site-dispositions.tsv'
FAMILIES={'xcf':(xcf,'migration/inventory/persistence-xcf-fields.json'),
          'mypaint':(mypaint,'migration/inventory/persistence-mypaint-fields.json'),
          'config':(config,'migration/inventory/persistence-config-fields.json')}
REQUIRED=('id','route','disposition','storage','version','saved_name','native_type','legacy_type','owner','default','absence','flags','reader','writer','legacy_reader','legacy_writer','evidence','assertion','test_file')
DISPOSITIONS={'saved-field','dynamic-schema','unchanged-standard','non-persisted'}

def sha(data): return hashlib.sha256(data).hexdigest()
def read_json(root,path): return json.loads((root/path).read_text())
def dump(data): return json.dumps(data,indent=2,ensure_ascii=False)+'\n'
def read_tsv(root,path): return list(csv.DictReader((root/path).open(),delimiter='\t'))
def tsv(rows,columns):
    out=io.StringIO(); writer=csv.DictWriter(out,columns,delimiter='\t',lineterminator='\n')
    writer.writeheader()
    for row in rows:
        writer.writerow({k:json.dumps(row[k],ensure_ascii=False,separators=(',',':')) if isinstance(row[k],(list,dict)) else row[k] for k in columns})
    return out.getvalue()

def source_checked_families(root):
    """Read family ledgers only after independently checking each against source.

    Every normal invocation performs the public family validators with fresh
    source reads; mutation controls call these same validators directly.
    """
    result={}; assertions={}
    for name,(module,path) in FAMILIES.items():
        data=read_json(root,path)
        exact(data.get('unresolved'),[],name+' unresolved')
        assertions[name]=module.validate_family(data,root)
        exact(set(assertions[name]),{f['assertion'] for f in data['fields']},name+' executed assertions')
        exact(len(assertions[name]),len(data['fields']),name+' unique assertion IDs')
        result[name]=copy.deepcopy(data)
    return result,assertions

def all_fields(families): return sorted([f for d in families.values() for f in d['fields']],key=lambda x:x['id'])
def all_candidates(families): return sorted([c for d in families.values() for c in d['candidates']],key=lambda x:x['legacy_site'])

def preset_domain(root,families):
    domain=copy.deepcopy(next(d for d in families['config']['schema_domains'] if d.get('id')=='tool-preset-options'))
    native=source(str(root),'app/paint/painter-mypaint-surface/gimp-painter-options.cpp')
    assert re.search(r'G_DEFINE_TYPE_WITH_CODE\s*\(\s*GimpPainterMybrushOptions\s*,\s*gimp_painter_mybrush_options\s*,\s*GIMP_TYPE_PAINT_OPTIONS\s*,',native)
    parents=next(d['inheritance'] for d in families['config']['schema_domains'] if d.get('revision')=='native')
    exact(parents['GimpPainterDeviceOptions'],'GIMP_TYPE_TOOL_OPTIONS','device options parent')
    exact(parents['GimpPaintOptions'],'GIMP_TYPE_TOOL_OPTIONS','paint options parent')
    domain['members']=sorted(set(domain['members'])|{f['id'] for f in families['mypaint']['fields'] if f['route']=='12'})
    domain['additional_inheritance']={'GimpPainterMybrushOptions':'GIMP_TYPE_PAINT_OPTIONS'}
    domain['distinct_existing_inheritance']={'GimpPainterDeviceOptions':parents['GimpPainterDeviceOptions'],'GimpPaintOptions':parents['GimpPaintOptions']}
    domain['member_rule']='Declared fields are inherited according to exact saved GType, not every field on every subclass. MyPaint option members augment the config-family domain; device and MyPaint owners remain distinct.'
    return domain

def validate_schema(root,families,assertions):
    fields=all_fields(families); byid={f['id']:f for f in fields}
    exact(len(byid),len(fields),'duplicate cross-family field IDs')
    routes=read_tsv(root,'migration/inventory/persistence-routes.tsv')
    route_ids={f'{i:02d}' for i in range(1,len(routes)+1)}
    exact(route_ids,{f'{i:02d}' for i in range(1,17)},'fixed existing16 routes')
    exact({f['route'] for f in fields},route_ids,'route coverage')
    all_assertions=[a for ids in assertions.values() for a in ids]
    exact(len(set(all_assertions)),len(fields),'cross-family unique executable assertions')
    for f in fields:
        for key in REQUIRED:
            assert key in f, (f['id'],'missing schema member',key)
            if key not in ('legacy_reader','legacy_writer'): assert f[key],(f['id'],'empty schema member',key)
        assert f['disposition'] in DISPOSITIONS-{'non-persisted'},(f['id'],'field disposition')
        for key in ('owner','default','absence','flags'):
            assert not re.search(r'\b(TODO|TBD|UNKNOWN_DEFAULT|unresolved default|see GParamSpec)\b',f[key],re.I),(f['id'],'ambiguous '+key)
        family=next(name for name,d in families.items() if f in d['fields'])
        assert f['assertion'] in assertions[family],(f['id'],'unexecuted assertion')
        exact(f['test_file'],'migration/tests/test_persistence_'+family+'_contract.py',f['id']+' test target')
        for side in ('reader','writer','legacy_reader','legacy_writer'):
            for anchor in f[side]:
                path,token=anchor.split('#',1)
                assert token in source(str(root),path,'legacy' if side.startswith('legacy') else 'native'),(f['id'],'unresolved '+side,anchor)
    census=read_tsv(root,'migration/inventory/persistence-sites.tsv')
    candidates=all_candidates(families); sites={c['legacy_site']:c for c in candidates}
    exact(len(sites),len(candidates),'duplicate candidate ownership')
    exact(set(sites),{c['legacy_site'] for c in census},'fixed syntax candidate population')
    for c in candidates:
        assert c['disposition'] in DISPOSITIONS and c['reason'],(c['legacy_site'],'candidate disposition/reason')
        if c['route']=='none':
            assert not c['fields'] and c['disposition'] in ('non-persisted','unchanged-standard'),c
        else:
            assert c['route'] in route_ids and c['fields'],c
            for fid in c['fields']:
                assert fid in byid,(c['legacy_site'],'unresolved field',fid)
                allowed={'app/core/gimptooloptions.c:318':('12','13'),'app/core/gimptooloptions.c:346':('12','13')}
                if byid[fid]['route']!=c['route']:
                    exact((c['route'],byid[fid]['route']),allowed.get(c['legacy_site']),c['legacy_site']+' inherited-context route '+fid)
    domain=preset_domain(root,families)
    assert all(fid in byid for fid in domain['members'])
    return fields,candidates,domain

def expected_manifest(root,families,assertions):
    fields,candidates,domain=validate_schema(root,families,assertions)
    sources={}; successors={}
    for data in families.values():
        for f in data['source_files']:
            key=(f['revision'],f['path']); digest=sha(source(str(root),f['path'],f['revision']).encode())
            transition = resolve_source_successor(root, *key, f['sha256'], digest)
            if transition: successors[key] = transition
            sources[key]=digest
        for f in data['fields']:
            for side in ('reader','writer','legacy_reader','legacy_writer'):
                for anchor in f[side]:
                    path=anchor.split('#')[0]; rev='legacy' if side.startswith('legacy') else 'native'
                    sources[(rev,path)]=sha(source(str(root),path,rev).encode())
            for ev in f['evidence']:
                sources[(ev['revision'],ev['path'])]=sha(source(str(root),ev['path'],ev['revision']).encode())
    enum_successor = enum_successor_provenance(root)
    if enum_successor:
        for provider in enum_successor['providers']:
            sources[('native', provider['path'])] = provider['successor_sha256']
    route_rows=read_tsv(root,'migration/inventory/persistence-routes.tsv')
    source_hashes=[dict(revision=rev,path=path,sha256=digest) for (rev,path),digest in sorted(sources.items())]
    tests=['migration/tests/persistence_delete_successor.py','migration/tests/persistence_enum_successor.py','migration/tests/persistence_successors.py','migration/tests/persistence_source.py','migration/tests/test_persistence_field_contract.py']+[f'migration/tests/test_persistence_{n}_contract.py' for n in FAMILIES]
    return dict(schema_version=1,task='10.001',scope='source-grounded saved field/type/default/owner/serialization contract across existing16 routes',
      base_public_commit=BASE,legacy_commit=LEGACY,
      inputs=[dict(path=p,sha256=sha((root/p).read_bytes())) for p in ['migration/inventory/persistence-routes.tsv','migration/inventory/persistence-sites.tsv']],
      families=[dict(name=n,path=p,sha256=sha((root/p).read_bytes()),test=f'migration/tests/test_persistence_{n}_contract.py',field_ids=sorted(f['id'] for f in families[n]['fields'])) for n,(m,p) in FAMILIES.items()],
      routes=[dict(id=f'{i:02d}',inventory=r,field_ids=sorted(f['id'] for f in fields if f['route']==f'{i:02d}')) for i,r in enumerate(route_rows,1)],
      cross_family_domains=[domain],sources=source_hashes,source_successors=[successors[k] for k in sorted(successors)],enum_successor=enum_successor,tests=[dict(path=p,sha256=sha((root/p).read_bytes())) for p in tests],
      counts=dict(contract_records=len(fields),syntax_candidates=len(candidates),routes=len(route_rows),by_disposition=dict(sorted(Counter(f['disposition'] for f in fields).items())),candidate_dispositions=dict(sorted(Counter(c['disposition'] for c in candidates).items()))),
      unresolved=[],limits=['Finite schema members and parameterized/opaque schemas are separate contract records, not a count of runtime instances or873 independent fields.','Source assertions do not establish runtime roundtrip, property ordering/duplicates, rendering, platform equivalence or later WBS completion.'])

def validate_manifest(actual,expected):
    # This is the same complete manifest comparison used by normal acceptance.
    exact(actual,expected,'unified manifest')

def mypaint_preview_negative_controls(root, family):
    from unittest.mock import patch
    controls = []
    def reject(label, fn):
        try: fn()
        except (AssertionError, KeyError) as error:
            controls.append(dict(id=label, rejected=True, reason=str(error)[:500]))
        else: raise AssertionError('negative control accepted: '+label)
    # Follow the shared PNG encoder and both callers. These are read-only
    # source overlays against the actual family validator, not count changes.
    real_source = mypaint.source
    path = 'app/core/gimppaintermybrush.cpp'
    original = real_source(str(root), path)
    for label, old, new in [
      ('encoder-error', 'if (!success) return false;', 'if (!success) return true;'),
      ('setter-bypass', 'if (!encode_icon (icon.get (), encoded, error)) return FALSE;', '/* bypass setter encoding */'),
      ('save-bypass', 'if (!encode_icon (icon.get (), encoded, error)) return false;', '/* bypass save encoding */'),
      ('save-no-icon-owner', 'auto icon = impl.icon;', 'auto *icon = impl.icon.get ();'),
      ('save-stale-preview', 'snapshot.set_preview_png_base64 (encoded);', '/* stale preview */'),
      ('save-source-mutation', 'snapshot.set_preview_png_base64 (encoded);', 'impl.resource.set_preview_png_base64 (encoded);')]:
        assert original.count(old) == 1, (label, 'ambiguous source mutation')
        changed = original.replace(old, new, 1)
        def overlay(r,p,revision='native'):
            return changed if p == path and revision == 'native' else real_source(r,p,revision)
        overlay.cache_clear = real_source.cache_clear
        # Match the synthetic source seal: rejection must be semantic, not
        # merely the expected fingerprint mismatch of an edited fixture.
        bad_family = copy.deepcopy(family)
        next(row for row in bad_family['source_files']
             if row['revision']=='native' and row['path']==path)['sha256'] = sha(changed.encode())
        with patch.object(mypaint, 'source', overlay):
            reject('mypaint:source:'+label, lambda:mypaint.validate_family(bad_family,root))
    return controls

def negative_controls(root,families,assertions,manifest):
    controls=[]
    def reject(label,fn):
        try: fn()
        except (AssertionError,KeyError) as error:
            controls.append(dict(id=label,rejected=True,reason=str(error)[:500]))
        else: raise AssertionError('negative control accepted: '+label)
    # Representative metadata changes exercise the actual public family
    # validators, without an unused snapshot-only comparator or count oracle.
    for name,(module,path) in FAMILIES.items():
        keys=('default','owner','flags','saved_name','native_type','legacy_type','reader','writer','assertion','test_file','route') if name=='xcf' else ('default','reader')
        for key in keys:
            bad=copy.deepcopy(families[name]); value=bad['fields'][0][key]
            bad['fields'][0][key]=['missing#anchor'] if isinstance(value,list) else 'MUTATED'
            reject(name+':metadata:'+key,lambda:module.validate_family(bad,root))
        bad=copy.deepcopy(families[name]);bad['fields'].pop()
        # A fabricated adjusted count must not rescue a missing source member.
        bad['counts']={'fields':len(bad['fields'])}
        reject(name+':field-omit-adjusted-count',lambda:module.validate_family(bad,root))
        bad=copy.deepcopy(families[name]);bad['candidates'].pop()
        reject(name+':candidate-omit',lambda:module.validate_family(bad,root))
        bad=copy.deepcopy(families[name]);bad['candidates'][0]['route']='99'
        reject(name+':candidate-route',lambda:module.validate_family(bad,root))
    bad=copy.deepcopy(families['config']);bad['schema_domains'][-1]['members']=[]
    reject('config:parameter-domain',lambda:config.validate_family(bad,root))
    bad=copy.deepcopy(families)
    next(c for c in bad['config']['candidates'] if c['fields'])['fields']=['MISSING_FIELD']
    reject('unified:unresolved-candidate-field',lambda:validate_schema(root,bad,assertions))
    bad=copy.deepcopy(families)
    c=next(c for c in bad['config']['candidates'] if c['route']=='12' and c['fields']);c['fields']=[families['mypaint']['fields'][0]['id']]
    reject('unified:wrong-cross-family-route',lambda:validate_schema(root,bad,assertions))
    bad=copy.deepcopy(families)
    for data in bad.values():data['fields']=[f for f in data['fields'] if f['route']!='09']
    reject('unified:missing-route',lambda:validate_schema(root,bad,assertions))
    wrong=copy.deepcopy(manifest);wrong['routes'].pop();wrong['counts']['routes']=len(wrong['routes'])
    reject('manifest:route-omit-adjusted-count',lambda:validate_manifest(wrong,manifest))
    wrong=copy.deepcopy(manifest);wrong['cross_family_domains'][0]['members']=[i for i in wrong['cross_family_domains'][0]['members'] if not i.startswith('mypaint.')]
    reject('manifest:cross-family-domain-omission',lambda:validate_manifest(wrong,manifest))
    # Source mutation uses a read-only overlay, never edits production files.
    from unittest.mock import patch
    real_source=xcf.source
    for label,path,old,new in [
      ('constructor-default','app/core/gimpitem.c','private->visible = TRUE;','private->visible = FALSE;'),
      ('capsule-type','app/xcf/painter-xcf-preserve.cpp','g_variant_new_uint32 (1)','g_variant_new_uint64 (1)')]:
        original=real_source(str(root),path)
        if old not in original:
            # Formatting-independent substitution, with exactly one required hit.
            pattern=r'\s*'.join(map(re.escape,old.split()))
            changed,count=re.subn(pattern,new,original,count=1);assert count,(label,'source mutation not applied')
        else:changed=original.replace(old,new,1)
        def overlay(r,p,revision='native'):
            return changed if p==path and revision=='native' else real_source(r,p,revision)
        overlay.cache_clear=real_source.cache_clear
        with patch.object(xcf,'source',overlay):
            reject('xcf:source:'+label,lambda:xcf.validate_family(families['xcf'],root))
    controls.extend(mypaint_preview_negative_controls(root, families['mypaint']))
    controls.extend(config.tool_group_negative_controls(root))
    controls.extend(config.json_resource_negative_controls(root))
    return controls

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--write-derived',action='store_true');parser.add_argument('--negative-controls',action='store_true');parser.add_argument('--report',type=Path)
    args=parser.parse_args();root=args.root
    source.cache_clear();families,assertions=source_checked_families(root)
    expected=expected_manifest(root,families,assertions)
    fields=all_fields(families);candidates=all_candidates(families)
    fields_text=tsv(fields,[k for k in REQUIRED if k!='evidence'])
    sites_text=tsv(candidates,['legacy_site','route','disposition','fields','reason'])
    if args.write_derived:
        (root/MANIFEST).write_text(dump(expected));(root/FIELD_VIEW).write_text(fields_text);(root/SITE_VIEW).write_text(sites_text)
    validate_manifest(read_json(root,MANIFEST),expected)
    exact((root/FIELD_VIEW).read_text(),fields_text,'field view')
    exact((root/SITE_VIEW).read_text(),sites_text,'candidate disposition view')
    controls=negative_controls(root,families,assertions,expected) if args.negative_controls else []
    report=dict(result='PASS',scope=expected['scope'],base_public_commit=BASE,legacy_commit=LEGACY,counts=expected['counts'],executed_assertions=assertions,negative_controls=controls,manifest_sha256=sha((root/MANIFEST).read_bytes()),unresolved=[])
    if args.report:args.report.write_text(dump(report))
    print(f"PASS: {len(fields)} source-checked contract records; {len(candidates)} exact candidate dispositions;16 routes; {len(controls)} rejected controls")

if __name__=='__main__':main()
