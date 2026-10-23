#!/usr/bin/env python3
"""Exercise additive routes using isolated copies of the real reviewed inputs."""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[4]
GENERATOR = 'app/painter/tests/headers/generate-probes.py'
module = importlib.util.spec_from_file_location('header_probes', ROOT/GENERATOR)
probes = importlib.util.module_from_spec(module)
module.loader.exec_module(probes)


class AdditionalRoutes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.snapshot = tempfile.TemporaryDirectory()
        cls.addClassCleanup(cls.snapshot.cleanup)
        cls.base = Path(cls.snapshot.name)
        _, manifest = probes.specification(ROOT, True)
        paths = set(manifest['source_sha256'])
        paths.update(str(p.relative_to(ROOT)) for p in (ROOT/'app').rglob('*.h')
                     if 'tests' not in p.relative_to(ROOT).parts)
        generated = json.loads((ROOT/probes.GENERATED_SCOPE).read_text())
        paths.update(row['generator'] for row in generated['source_written'])
        for path in paths:
            destination = cls.base/path
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(ROOT/path, destination)

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)/'source'
        shutil.copytree(self.base, self.root)
        self.inventory = json.loads((self.root/probes.ADDITIONAL_SCOPE).read_text())
        self.row = copy.deepcopy(self.inventory['routes'][0])

    def save(self, value, path=probes.ADDITIONAL_SCOPE):
        (self.root/path).write_text(json.dumps(value))

    def reject(self, inventory, message):
        self.save(inventory)
        with self.assertRaisesRegex(ValueError, message):
            probes.specification(self.root, False)

    def test_historical_inventory_and_probe_names_are_preserved(self):
        # These three inventories, including their baseline source hashes and
        # legacy assignments, are not an extension point for this feature.
        expected = {
            probes.C_SCOPE: '31cf1b05b6f8d5e8a8c89f66136b036caa675dba6f176c61e0a2b0255cb01073',
            probes.CPP_SCOPE: 'bca3774f0ae0099a99a2508a6f8a6dcbf725a30b83dd7823ca06d8dc14886568',
            probes.GENERATED_SCOPE: '3c660bf85cbaab938cd153ccdbbcab21c781b6d38dabc0055fd1ed20a43c25d1',
        }
        for path, digest in expected.items():
            self.assertEqual(probes.sha((self.root/path).read_bytes()), digest)
        routes = json.loads((self.root/probes.C_SCOPE).read_text())['routes']
        self.assertEqual(len(routes), 103)
        for http, old_count, private_count in [(False, 357, 19), (True, 359, 21)]:
            with self.subTest(http=http):
                sources, manifest = probes.specification(self.root, http)
                additions = [p for p in sources if p['label'] == 'additional-reviewed-header']
                historical = [p for p in sources if p['label'] != 'additional-reviewed-header']
                self.assertEqual(len(manifest['public_targets']), 102)
                self.assertEqual(len(manifest['private_cpp_targets']), private_count)
                self.assertEqual(len(manifest['generated_source_targets']), 63)
                self.assertEqual(len(historical), old_count)
                self.assertEqual(len(additions), 2*len(self.inventory['routes']))
                self.assertEqual(manifest['additional_public_targets'],
                                 sorted(row['path'] for row in self.inventory['routes']))
                self.assertEqual(manifest['additional_reviewed_routes'],
                                 sorted(self.inventory['routes'], key=lambda row: row['path']))
                self.assertIn(probes.ADDITIONAL_SCOPE, manifest['source_sha256'])
                for index, source in enumerate(historical):
                    self.assertEqual(source['name'], 'probe-%03d.%s' % (index, source['language']))
                for row in self.inventory['routes']:
                    selected = [p for p in additions if p['header'] == row['path']]
                    self.assertEqual([p['language'] for p in selected], ['c', 'cpp'])
                    for source in selected:
                        self.assertEqual(source['body'].count(probes.include(row['path'])), 2)
                        self.assertNotIn('extern "C"', source['body'])
                    self.assertIn(row['path'], manifest['source_sha256'])

    def test_tool_options_changed_baseline_routes_remain_required(self):
        paths = ['app/tools/gimpalignoptions.h',
                 'app/tools/gimprectangleoptions.h',
                 'app/tools/gimpvectoroptions.h']
        baseline = {row['path']: row['baseline_blob'] for row in
                    json.loads((self.root/probes.C_SCOPE).read_text())[
                        'all_baseline_app_headers']}
        for path in paths:
            with self.subTest(path=path):
                row = next(row for row in self.inventory['routes']
                           if row['path'] == path)
                self.assertEqual(row['kind'], 'changed-baseline')
                self.assertEqual(row['baseline_blob'], baseline[path])
                self.assertEqual(row['context'], 'core')
                value = copy.deepcopy(self.inventory)
                value['routes'] = [row for row in value['routes']
                                   if row['path'] != path]
                self.reject(value, 'changed C header is outside the reviewed closure: '+path)

    def test_missing_inventory_is_rejected(self):
        (self.root/probes.ADDITIONAL_SCOPE).unlink()
        with self.assertRaises(FileNotFoundError):
            probes.specification(self.root, False)

    def test_missing_route_is_rejected_even_with_other_reviewed_routes(self):
        unreviewed = self.root/'app/widgets/unreviewed-addition.h'
        unreviewed.write_text('/* not in the reviewed inventory */\n')
        with self.assertRaisesRegex(ValueError, 'needs a reviewed compile route'):
            probes.specification(self.root, False)

    def test_deleted_route_is_rejected(self):
        self.inventory['routes'].pop(0)
        self.reject(self.inventory, 'needs a reviewed compile route')

    def test_duplicate_route_is_rejected(self):
        self.inventory['routes'].append(self.row)
        self.reject(self.inventory, 'duplicate additional C-header route')

    def test_missing_header_is_rejected(self):
        (self.root/self.row['path']).unlink()
        with self.assertRaisesRegex(ValueError, 'missing or outside source'):
            probes.specification(self.root, False)

    def test_schema_and_unreviewed_rows_are_rejected(self):
        invalid = [None, [], {}, {'format': True, 'routes': []},
                   {'format': 2, 'routes': []}, {'format': 1, 'routes': {}},
                   {'format': 1, 'routes': [], 'skip': True}]
        for value in invalid:
            with self.subTest(value=value):
                self.reject(value, 'invalid additional C-header route inventory')
        for field in ('path', 'kind', 'context', 'prerequisites', 'review'):
            value = copy.deepcopy(self.inventory)
            del value['routes'][0][field]
            with self.subTest(missing=field):
                self.reject(value, 'invalid additional C-header route fields')
        for review in ('', '  ', None, False, []):
            value = copy.deepcopy(self.inventory)
            value['routes'][0]['review'] = review
            with self.subTest(review=review):
                self.reject(value, 'needs a review')

    def test_invalid_paths_contexts_and_fields_are_rejected(self):
        for path in ('/tmp/header.h', '../app/header.h', 'app/../header.h',
                     'app//header.h', 'app/./header.h', 'app/tests/header.h',
                     'libgimp/header.h', 'app/header.hpp', 'app/header.h\n',
                     'app/"header.h', 'app/\\header.h', None):
            value = copy.deepcopy(self.inventory)
            value['routes'][0]['path'] = path
            with self.subTest(path=path):
                self.reject(value, 'invalid additional C-header path')
        for context in ('unknown', '', None, ['core']):
            value = copy.deepcopy(self.inventory)
            value['routes'][0]['context'] = context
            with self.subTest(context=context):
                self.reject(value, 'invalid additional C-header context')
        value = copy.deepcopy(self.inventory)
        value['routes'][0]['languages'] = ['cpp']
        self.reject(value, 'invalid additional C-header route fields')

    def test_historical_headers_cannot_be_laundered_as_additions(self):
        for path in ('app/widgets/gimpeditor.h', 'app/core/gimp.h', probes.VISIBILITY):
            value = copy.deepcopy(self.inventory)
            value['routes'][0]['path'] = path
            with self.subTest(path=path):
                self.reject(value, 'additional C-header route is not new')

    def test_baseline_identity_and_category_are_required(self):
        index = next(i for i, row in enumerate(self.inventory['routes'])
                     if row['kind'] == 'changed-baseline')
        for identity in ('0'*40, '', None):
            value = copy.deepcopy(self.inventory)
            value['routes'][index]['baseline_blob'] = identity
            with self.subTest(identity=identity):
                self.reject(value, 'baseline identity mismatch')
        value = copy.deepcopy(self.inventory)
        del value['routes'][index]['baseline_blob']
        self.reject(value, 'invalid additional C-header route fields')
        value = copy.deepcopy(self.inventory)
        value['routes'][0]['kind'] = 'changed-baseline'
        value['routes'][0]['baseline_blob'] = '0'*40
        self.reject(value, 'baseline identity mismatch')
        value = copy.deepcopy(self.inventory)
        value['routes'][0]['kind'] = 'unchecked'
        self.reject(value, 'invalid additional C-header route fields')

    def test_removed_baseline_extension_is_rejected(self):
        path = next(row['path'] for row in self.inventory['routes']
                    if row['kind'] == 'changed-baseline')
        with (self.root/path).open('a') as output:
            output.write('\n/* make the baseline-extension control explicit */\n')
        self.inventory['routes'] = [row for row in self.inventory['routes']
                                    if row['kind'] != 'changed-baseline']
        self.reject(self.inventory, 'outside the reviewed closure')

    def test_invalid_prerequisites_are_rejected(self):
        for prerequisites in (None, 'app/widgets/gimpeditor.h', [False],
                              [self.row['path']], ['app/widgets/not-reviewed.h'],
                              ['app/widgets/gimpeditor.h']*2):
            value = copy.deepcopy(self.inventory)
            value['routes'][0]['prerequisites'] = prerequisites
            with self.subTest(prerequisites=prerequisites):
                self.reject(value, 'invalid additional C-header prerequisites')

    def test_symlink_outside_source_is_rejected(self):
        header = self.root/self.row['path']
        header.unlink()
        external = Path(self.temporary.name)/'external.h'
        external.write_text('/* external */\n')
        header.symlink_to(external)
        with self.assertRaisesRegex(ValueError, 'missing or outside source'):
            probes.specification(self.root, False)

    def test_scope_argument_must_point_to_reviewed_inventory(self):
        with self.assertRaisesRegex(ValueError, 'unexpected reviewed scope path'):
            probes.specification(self.root, False, additional_scope=self.root/'unreviewed.json')

    def test_changed_baseline_header_still_needs_historical_closure(self):
        _, manifest = probes.specification(self.root, False)
        scope = json.loads((self.root/probes.C_SCOPE).read_text())
        untouched = next(row['path'] for row in scope['all_baseline_app_headers']
                         if row['path'] not in manifest['source_sha256']
                         and 'tests' not in Path(row['path']).parts
                         and (self.root/row['path']).is_file())
        with (self.root/untouched).open('a') as output:
            output.write('\n/* unreviewed baseline change */\n')
        with self.assertRaisesRegex(ValueError, 'outside the reviewed closure'):
            probes.specification(self.root, False)

    def test_reviewed_core_extension_gets_both_probes_and_source_hash(self):
        path = 'app/widgets/reviewed-extension-control.h'
        (self.root/path).write_text('#pragma once\nG_BEGIN_DECLS\nvoid control (void);\nG_END_DECLS\n')
        self.inventory['routes'].append({'path': path, 'kind': 'new', 'context': 'core',
                                         'prerequisites': [],
                                         'review': 'Isolated extension control.'})
        self.save(self.inventory)
        sources, manifest = probes.specification(self.root, False)
        selected = [p for p in sources if p['header'] == path]
        self.assertEqual([p['language'] for p in selected], ['c', 'cpp'])
        self.assertTrue(all('#include "config.h"' in p['body'] for p in selected))
        self.assertTrue(all('extern "C"' not in p['body'] for p in selected))
        self.assertIn(path, manifest['source_sha256'])
        before = manifest['source_sha256'][path]
        with (self.root/path).open('a') as output:
            output.write('\n/* force stale source detection */\n')
        _, changed = probes.specification(self.root, False)
        self.assertNotEqual(before, changed['source_sha256'][path])


if __name__ == '__main__':
    unittest.main(verbosity=2)
