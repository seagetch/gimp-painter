#!/usr/bin/env python3
import argparse,pathlib,json,hashlib,csv
p=argparse.ArgumentParser();p.add_argument('--check-source',action='store_true');a=p.parse_args();E=pathlib.Path(__file__).resolve().parent;R=E.parents[2];read=lambda n:json.loads((E/n).read_text())
rows=read('probe-results.json');assert len(rows)==18
assert len({(x['variant'],x['language'],x['order']) for x in rows})==18
for x in rows:
 expected=x['variant']=='current' or (x['variant']=='baseline' and x['order']!='opaque-only') or (x['variant']=='wrong-tag' and x['order']=='opaque-only')
 assert x['expected_success']==expected and (x['returncode']==0)==expected
 if not expected:assert 'GimpMypaintOptions' in x['stderr']
assert sum(x['returncode']==0 for x in rows)==12
h=read('header-compile-result.json');assert h['status']=='PASS' and h['compiled_probes']==423
for row in read('source-manifest.json'):
 assert hashlib.sha256((E/'tested-sources'/row['path']).read_bytes()).hexdigest()==row['sha256']
 if a.check_source:assert hashlib.sha256((R/row['path']).read_bytes()).hexdigest()==row['sha256'],row['path']
d=read('original-duty.json');assert d['original']['child_id']=='01.013/hunk-001006';assert d['original'] in list(csv.DictReader((R/'migration/inventory/hunk-wbs.tsv').open(),delimiter='\t'))
assert 'GimpMypaintOptions;' not in (E/'baseline-paint-types.h').read_text()
print('PASS: original opaque name;18 C/C++ include-order probes with6 expected failures;423 headers; original duty/current sources')
