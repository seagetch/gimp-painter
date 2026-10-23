#!/usr/bin/env python3
"""Verify the bounded native scalar/static-string review, not feature completion."""
import collections, hashlib, json, pathlib, sys
HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import check_painter_legacy_adapters as gate

def check():
    review = json.loads((HERE / 'review.json').read_text())
    assert review['status'] == 'REVIEWED_NATIVE_SCALARS_AND_STATIC_METADATA'
    assert len(review['rows']) == 10 and sum(r['count'] for r in review['rows']) == 14
    policy = json.loads((ROOT / 'migration/contracts/legacy-adapter-removal.json').read_text())
    for row in review['rows']:
        assert policy['data_sites'].count(row) == 1
        assert row['path'] in policy['reviews'][row['provenance']]['paths']
    for row in review['superseded_static_field_tokens']:
        assert row not in policy['data_sites']
    for path, expected in review['existing_evidence_sha256'].items():
        assert hashlib.sha256((ROOT / path).read_bytes()).hexdigest() == expected, path
    for path, expected in review['reviewed_source_sha256'].items():
        source = (ROOT / path).read_bytes()
        assert hashlib.sha256(source).hexdigest() == expected, path
        rows = gate.inspect_source(source.decode(), path)['data_sites']
        actual = collections.Counter((r['operation'], r['sha256']) for r in rows)
        for row in review['rows']:
            if row['path'] == path:
                assert actual[(row['operation'], row['sha256'])] == row['count']
    before = json.loads((HERE / 'baseline-gate.json').read_text())
    after = json.loads((HERE / 'final-gate.json').read_text())
    assert before['status'] == 'FAIL' and len(before['errors']) == 12
    assert after['status'] == 'PASS' and not after['errors']
    assert after['source_commit'] == review['source_commit']
    print('PASS: 10 exact fingerprints/14 native calls; static committed-source gate; historical runtime evidence explicitly reused')

if __name__ == '__main__':
    check()
