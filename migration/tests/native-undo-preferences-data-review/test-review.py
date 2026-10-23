#!/usr/bin/env python3
"""Review-specific controls use the unchanged production source gate."""
import collections, copy, importlib.util, json, pathlib, sys, tempfile, unittest
HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
import check_painter_legacy_adapters as gate

class NativeReviewControls(unittest.TestCase):
    def test_exact_sites_and_mutations(self):
        review = json.loads((HERE / 'review.json').read_text())
        full = json.loads((ROOT / 'migration/contracts/legacy-adapter-removal.json').read_text())
        for entry in review['rows']:
            source = (ROOT / entry['path']).read_text()
            token_rows = gate.inspect_source(source, entry['path'])['data_sites']
            call = next(r for r in token_rows if r['sha256'] == entry['sha256'])
            text = ' '.join(value for _, value in call['tokens']) + ';'
            with self.subTest(fingerprint=entry['sha256']), tempfile.TemporaryDirectory() as temp:
                root = pathlib.Path(temp); path = root / entry['path']; path.parent.mkdir(parents=True)
                path.write_text(text * entry['count'])
                policy = {'syntax_exceptions': [], 'dormant_exceptions': [],
                          'reviews': full['reviews'], 'data_sites': [entry]}
                self.assertEqual(gate.scan(root, policy)['status'], 'PASS')
                path.write_text(text * (entry['count'] + 1))
                self.assertEqual(gate.scan(root, policy)['status'], 'FAIL')
                path.write_text(text.replace('"', '"changed-', 1) * entry['count'])
                self.assertEqual(gate.scan(root, policy)['status'], 'FAIL')
                path.write_text(text * entry['count'])
                missing = copy.deepcopy(policy); missing['data_sites'] = []
                self.assertEqual(gate.scan(root, missing)['status'], 'FAIL')

if __name__ == '__main__':
    unittest.main()
