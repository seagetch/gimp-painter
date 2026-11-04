import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import matched_resources as m


class ResourcesTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='private-secret-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source, self.build, self.profile, self.catalogs = [self.root / n for n in ('source','build','profile','locale')]
        self.env = {'GIMP_TESTING_ABS_TOP_SRCDIR': str(self.source),
                    'GIMP_TESTING_ABS_TOP_BUILDDIR': str(self.build),
                    'GIMP_TESTING_PLUGINDIRS': str(self.build / 'plug-ins/common'),
                    'GIMP3_LOCALEDIR': str(self.catalogs), 'HOME': str(self.profile / 'home'),
                    'XDG_CACHE_HOME': str(self.profile / 'cache'),
                    'GIMP_TESTING_SESSIONRC_NAME': 'sessionrc-native-oncanvas',
                    'PRIVATE_SECRET': 'DO_NOT_EXPORT'}
        for name in ('data','themes','gimp-data','menus','app/tests/gimpdir'):
            self.write(self.source / name / 'input', b'input')
        self.write(self.build / 'app/tests/gimp-test-icon-theme/index.theme', b'icons')
        for name in ('image-menu.ui', 'dockable-menu.ui'):
            self.write(self.build / 'menus' / name, b'menu')
        self.write(self.source / 'plug-ins/common/meson.build', b'meson')
        self.write(self.source / 'plug-ins/common/convolution-matrix-kernel.h', b'header')
        nodes = []
        for p in m.PLUGINS:
            path = self.build / 'plug-ins/common' / p
            self.write(path, b'elf-fixture'); path.chmod(0o700)
            self.write(self.source / 'plug-ins/common' / (p + '.c'), b'source')
            nodes.append('(plug-in-def ' + json.dumps(str(path)) + ' 123 ' + ' '.join('(proc-def ' + json.dumps(proc) + ' 1 "help")' for proc in m.PROCEDURES[p]) + ')')
        self.cache_directory = self.profile / 'native-output'
        self.cache = self.cache_directory / 'pluginrc'
        self.write(self.cache, ('# private cache\n(protocol-version 1)\n' + '\n'.join(nodes)).encode())
        self.locale_fixture()
        deps = self.root / 'deps'
        font = deps / 'usr/share/fonts/test.ttf'; conf = deps / 'etc/fonts/fonts.conf'
        schema = deps / 'usr/share/glib-2.0/schemas'
        self.write(font, b'font'); self.write(conf, b'<fontconfig/>')
        self.write(schema / 'gschemas.compiled', b'schema')
        evidence = self.root / 'font-evidence.json'
        self.write(evidence, json.dumps({'font_files':[str(font)], 'fontconfig_files':[str(conf)], 'schema_dirs':[str(schema)]}).encode())
        self.env.update(GIMP_DEPS_ROOT=str(deps), FONTCONFIG_FILE=str(conf),
                        GSETTINGS_SCHEMA_DIR=str(schema), XDG_DATA_DIRS=str(deps / 'usr/share'))
        self.evidence = {'fc_list_stdout':str(font) + '\n',
                         'fc_conflist_stdout':'+ ' + str(conf) + ': test configuration\n',
                         'ninja_alias_queries':{name:name + ':\n  input: phony\n    po/en/catalog.mo\n  outputs:\n' for name in m.ALIASES},
                         'cache_absent_preflight':True}

    def write(self, p, data):
        p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(data)

    def locale_fixture(self):
        helper = self.source / 'tools/run-painter-frozen-aggregate.py'
        self.write(helper, b'# existing helper fixture')
        targets = [{'name': a, 'id': 'id-' + a, 'type':'alias'} for a in m.ALIASES]
        rel = 'en/LC_MESSAGES/gimp30.mo'; src = 'po/en/LC_MESSAGES/gimp30.mo'
        self.write(self.catalogs / rel, b'catalog'); self.write(self.build / src, b'catalog')
        targets.append({'id':'catalog-id','filename':[str(self.build / src)],'installed':True})
        names = ('config.h','meson-info/intro-buildoptions.json','meson-info/intro-install_plan.json',
                 'meson-info/intro-installed.json','meson-info/intro-targets.json')
        for n in names: self.write(self.build / n, b'{}')
        self.write(self.build / names[-1], json.dumps(targets).encode())
        self.write(self.build / 'extensions/goat.mo', b'extension')
        row = dict(m.record(self.catalogs / rel), source=src, target_id='catalog-id')
        manifest = {'helper_sha256':m.record(helper)['sha256'],
                    'metadata_sha256':{n:m.record(self.build / n)['sha256'] for n in names},
                    'catalogs':{rel:row}, 'catalog_count':1,
                    'excluded_extension_catalog_outputs':['extensions/goat.mo']}
        self.write(self.catalogs.with_name('locale.json'), json.dumps(manifest).encode())

    def test_alias_error_has_only_finite_substage(self):
        self.evidence['ninja_alias_queries'] = {name:name + ':\n  input: phony\n    | ../private-secret\n  outputs:\n' for name in m.ALIASES}
        with self.assertRaises(m.ResourceError) as caught:
            m.locales(self.source,self.build,self.catalogs,self.evidence)
        self.assertEqual(str(caught.exception),'resource-relative-name-invalid')
        self.assertEqual(caught.exception.substage,'catalog-alias-inputs')

    def test_ninja_query_dependency_kinds(self):
        for prefix in ('', '| ', '|| '):
            for absolute in (False, True):
                value = str(self.build / 'po/en/catalog.mo') if absolute else 'po/en/catalog.mo'
                self.evidence['ninja_alias_queries'] = {name:name + ':\n  input: phony\n    ' + prefix + value + '\n  outputs:\n' for name in m.ALIASES}
                result = m.locales(self.source,self.build,self.catalogs,self.evidence)
                self.assertTrue(all(row['dependencies'] == ['po/en/catalog.mo'] for row in result['aliases'].values()))

    def test_ninja_query_malformed_or_escaping_dependency_rejected(self):
        for value in ('| ../escape.mo', '|| /outside/catalog.mo', '||| po/a.mo', '|', '||', '| | po/a.mo', ' po/a.mo', 'po/a.mo ', '| po/a mo'):
            with self.subTest(value=value):
                self.evidence['ninja_alias_queries'] = {name:name + ':\n  input: phony\n    ' + value + '\n  outputs:\n' for name in m.ALIASES}
                with self.assertRaises(m.ResourceError):
                    m.locales(self.source,self.build,self.catalogs,self.evidence)

    def test_ninja_query_wrong_section_or_indentation_rejected(self):
        for body in ('  input: phony\n  | po/a.mo', '  input: CUSTOM_COMMAND\n    po/a.mo', '  input: phony\n  validations:\n    po/a.mo'):
            self.evidence['ninja_alias_queries'] = {name:name + ':\n' + body + '\n  outputs:\n' for name in m.ALIASES}
            with self.assertRaises(m.ResourceError):
                m.locales(self.source,self.build,self.catalogs,self.evidence)

    def test_receipt_no_private_paths_or_environment(self):
        receipt = m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,cache_directory=self.cache_directory)
        public = json.dumps(receipt)
        self.assertNotIn(str(self.root), public)
        self.assertNotIn('DO_NOT_EXPORT', public)
        self.assertTrue(receipt['comparison_eligible'])
        self.assertFalse(receipt['font_schema_inputs']['live_pango_font_selection_proven'])
        self.assertEqual(set(receipt['plugins']['registrations']), set(m.PLUGINS))

    def test_plugin_missing_and_extra_executables(self):
        extra = self.build / 'plug-ins/common/extra'; self.write(extra,b'extra');extra.chmod(0o700)
        with self.assertRaisesRegex(m.ResourceError,'^plugin-executable-inventory$'):
            m.plugins(self.source,self.build,self.cache_directory)
        extra.unlink(); (self.build / 'plug-ins/common/blinds').unlink()
        with self.assertRaisesRegex(m.ResourceError,'^plugin-executable-inventory$'):
            m.plugins(self.source,self.build,self.cache_directory)

    def test_plugin_cache_required(self):
        self.cache.unlink()
        with self.assertRaises(m.ResourceError): m.plugins(self.source,self.build,self.cache_directory)

    def test_plugin_wrong_provider_and_missing_registration(self):
        text = self.cache.read_text()
        self.cache.write_text(text.replace(str(self.build / 'plug-ins/common/blinds'), '/private/wrong/blinds'))
        with self.assertRaisesRegex(m.ResourceError,'^plugin-registration-provider$'):
            m.plugins(self.source,self.build,self.cache_directory)
        self.cache.write_text('(protocol-version 1)')
        with self.assertRaisesRegex(m.ResourceError,'^plugin-registration-inventory$'):
            m.plugins(self.source,self.build,self.cache_directory)

    def test_plugin_timestamp_not_part_of_comparison(self):
        a = m.plugins(self.source,self.build,self.cache_directory)
        self.cache.write_text(self.cache.read_text().replace(' 123 ', ' 456 '))
        self.assertEqual(a,m.plugins(self.source,self.build,self.cache_directory))

    def test_malformed_cache(self):
        for text in ('(', ')', '(a "broken)', 'word', '(' * 65):
            with self.subTest(text=text), self.assertRaises(m.ResourceError): m.sexps(text.encode())

    def test_catalog_mutation(self):
        self.write(self.catalogs / 'en/LC_MESSAGES/gimp30.mo', b'changed')
        with self.assertRaisesRegex(m.ResourceError,'^catalog-content-mismatch$'):
            m.locales(self.source,self.build,self.catalogs,self.evidence)

    def test_catalog_metadata_mutation(self):
        self.write(self.build / 'config.h', b'changed')
        with self.assertRaisesRegex(m.ResourceError,'^catalog-metadata-mismatch$'):
            m.locales(self.source,self.build,self.catalogs,self.evidence)

    def test_path_escape_and_symlink(self):
        with self.assertRaises(m.ResourceError): m.under(self.source, '../secret')
        (self.source / 'data/escape').symlink_to(self.cache)
        with self.assertRaisesRegex(m.ResourceError,'^resource-symlink-invalid$'): m.inventory(self.source / 'data')

    def test_missing_font_evidence_cannot_be_boolean_override(self):
        self.evidence['fc_list_stdout'] = ''
        self.evidence['font_resolution_proven'] = True
        with self.assertRaisesRegex(m.ResourceError,'^font-resolution-inventory-invalid$'):
            m.font_schema_inputs(self.env,self.evidence)

    def test_alias_missing_and_empty(self):
        self.evidence['ninja_alias_queries'][m.ALIASES[0]] = m.ALIASES[0] + ':\n  input: phony\n  outputs:\n'
        with self.assertRaisesRegex(m.ResourceError,'^catalog-alias-unproven$'):
            m.locales(self.source,self.build,self.catalogs,self.evidence)

    def test_freshness_required(self):
        self.evidence['cache_absent_preflight'] = False
        with self.assertRaisesRegex(m.ResourceError,'^plugin-cache-freshness-unproven$'):
            m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,cache_directory=self.cache_directory)

    def test_cell_binary_hash_excluded_from_comparison(self):
        a = m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,cache_directory=self.cache_directory)
        self.write(self.build / 'plug-ins/common/blinds', b'other instrumented binary')
        b = m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,cache_directory=self.cache_directory)
        self.assertNotEqual(a['plugin_binaries'],b['plugin_binaries'])
        self.assertEqual(a['comparison_inventory'],b['comparison_inventory'])

    def test_wrong_procedure_inventory(self):
        self.cache.write_text(self.cache.read_text().replace('plug-in-blinds', 'wrong-procedure'))
        with self.assertRaisesRegex(m.ResourceError,'^plugin-registration-invalid$'):
            m.plugins(self.source,self.build,self.cache_directory)

    def test_runtime_binding_and_finite_error(self):
        self.env['HOME'] = '/private/secret'
        with self.assertRaisesRegex(m.ResourceError,'^resource-runtime-binding-mismatch$'):
            m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,cache_directory=self.cache_directory)

    def test_archive_binding_required(self):
        for wrong in (self.build / 'app/tests/gimpdir-output', self.profile / 'other-output'):
            with self.subTest(wrong=wrong), self.assertRaisesRegex(m.ResourceError, '^plugin-cache-archive-binding-mismatch$'):
                m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,
                                    cache_directory=wrong)

    def test_archive_symlink_rejected(self):
        moved = self.profile / 'moved-output'
        self.cache_directory.rename(moved)
        self.cache_directory.symlink_to(moved, target_is_directory=True)
        with self.assertRaisesRegex(m.ResourceError, '^plugin-cache-archive-binding-mismatch$'):
            m.runtime_resources(self.source,self.build,self.profile,self.catalogs,self.env,self.evidence,
                                cache_directory=self.cache_directory)

    def test_selected_data_inventory_and_order(self):
        share = Path(self.env['GIMP_DEPS_ROOT']) / 'usr/share'
        self.write(share / 'icons/theme/index.theme', b'icon')
        prefix = self.root / 'cell-prefix'
        self.env['GIMP_BUILD_PREFIX'] = str(prefix)
        self.env['XDG_DATA_DIRS'] = str(prefix / 'share') + ':' + str(share)
        result = m.selected_data_resources(self.env)
        self.assertEqual([x['provider'] for x in result], ['cell-prefix-share','dependency-share'])
        self.assertFalse(result[0]['inputs']['icons']['present'])
        self.assertTrue(result[1]['inputs']['icons']['present'])
        self.assertNotIn(str(self.root), json.dumps(result))

    def test_unknown_data_provider_rejected(self):
        self.env['XDG_DATA_DIRS'] = str(self.root / 'unexpected')
        with self.assertRaisesRegex(m.ResourceError, '^data-provider-unapproved$'):
            m.selected_data_resources(self.env)

    def test_cell_prefix_assets_rejected(self):
        prefix = self.root / 'cell-prefix'
        self.env['GIMP_BUILD_PREFIX'] = str(prefix)
        self.env['XDG_DATA_DIRS'] = str(prefix / 'share')
        self.write(prefix / 'share/icons/extra', b'not approved')
        with self.assertRaisesRegex(m.ResourceError, '^data-cell-prefix-provider-unapproved$'):
            m.selected_data_resources(self.env)

    def test_internal_data_directory_symlink_witness(self):
        icons = self.root / 'icons'
        self.write(icons / 'actual/cursors/arrow', b'cursor')
        (icons / 'default').symlink_to('actual', target_is_directory=True)
        result = m.data_inventory(icons)
        self.assertEqual(result['files']['actual/cursors/arrow'], result['files']['default/cursors/arrow'])
        self.assertEqual(result['links']['default'], {'target_relative':'actual','kind':'directory'})
        self.assertNotIn(str(self.root), json.dumps(result))

    def test_data_directory_link_cycle(self):
        icons = self.root / 'icons'
        self.write(icons / 'actual/file', b'icon')
        (icons / 'actual/loop').symlink_to('..', target_is_directory=True)
        with self.assertRaisesRegex(m.ResourceError, '^data-directory-cycle$'):
            m.data_inventory(icons)

    def test_data_directory_external_and_dangling_links(self):
        icons = self.root / 'icons'
        self.write(icons / 'actual/file', b'icon')
        self.write(self.root / 'external/file', b'external')
        for target in (self.root / 'external', self.root / 'missing'):
            link = icons / 'bad'
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(m.ResourceError, '^resource-symlink-invalid$'):
                m.data_inventory(icons)
            link.unlink()

    def test_data_alias_expansion_is_bounded(self):
        icons = self.root / 'icons'
        self.write(icons / 'actual/file', b'icon')
        (icons / 'alias').symlink_to('actual', target_is_directory=True)
        with patch.object(m, 'MAX_FILES', 2), self.assertRaisesRegex(m.ResourceError, '^resource-inventory-limit$'):
            m.data_inventory(icons)

    def test_data_directory_depth_is_bounded(self):
        root = self.root / 'deep-icons'
        self.write(root.joinpath(*(['d'] * 66)) / 'file', b'icon')
        with self.assertRaisesRegex(m.ResourceError, '^data-directory-depth-limit$'):
            m.data_inventory(root)

    def test_duplicate_json_key(self):
        p = self.root / 'bad.json';self.write(p,b'{"x":1,"x":2}')
        with self.assertRaisesRegex(m.ResourceError,'^resource-json-duplicate$'):m.json_read(p)

class LegacyBrushNamesTest(unittest.TestCase):
    def test_default_grammar_unchanged(self):
        for name in list(m.LEGACY_SOURCE_BRUSHES) + ['a(1).myb', 'x%20y', '../x', '/absolute', 'a/../x', 'a//b']:
            with self.assertRaises(m.ResourceError): m.relative(name)

    def test_exact_names_and_content_only(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t)
            for name, expected in m.LEGACY_SOURCE_BRUSHES.items():
                p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b'fixture')
            with patch.object(m,'record',side_effect=lambda p: dict(m.LEGACY_SOURCE_BRUSHES[p.relative_to(root).as_posix()])):
                self.assertEqual(m.inventory(root,legacy_source_brushes=True),m.LEGACY_SOURCE_BRUSHES)
                with self.assertRaises(m.ResourceError): m.inventory(root)
            with self.assertRaisesRegex(m.ResourceError,'^resource-evidence-invalid$'):
                m.inventory(root,legacy_source_brushes=True)

    def test_unknown_punctuation_still_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            root=Path(t);(root/'unknown(1).myb').write_bytes(b'x')
            with self.assertRaisesRegex(m.ResourceError,'^resource-relative-name-invalid$'):
                m.inventory(root,legacy_source_brushes=True)

    def test_legacy_symlink_and_directory_rejected(self):
        for kind in ('symlink','directory'):
            with tempfile.TemporaryDirectory() as t:
                root=Path(t);name=next(iter(m.LEGACY_SOURCE_BRUSHES));p=root/name;p.parent.mkdir(parents=True)
                if kind=='symlink':
                    target=root/'valid';target.write_bytes(b'x');p.symlink_to(target)
                else:p.mkdir()
                with self.assertRaisesRegex(m.ResourceError,'^resource-evidence-invalid$'):
                    m.inventory(root,legacy_source_brushes=True)

if __name__ == '__main__': unittest.main()
