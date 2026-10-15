#!/usr/bin/env python3
"""Negative/positive controls for the legacy adapter source gate."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

SPEC = importlib.util.spec_from_file_location('adapters', Path(__file__).resolve().parents[1] / 'check_painter_legacy_adapters.py')
A = importlib.util.module_from_spec(SPEC); sys.modules[SPEC.name] = A; SPEC.loader.exec_module(A)


def inspect(text):
    return A.inspect_source(text, 'app/example.cpp')


class LexicalTests(unittest.TestCase):
    def test_old_identifiers(self):
        for name in A.OLD_NAMES:
            with self.subTest(name=name): self.assertTrue(inspect(name + ' *value;')['findings'])

    def test_interface_comments_and_lines(self):
        rows = inspect('/* line 1 */\nCloneLayerInterface /*gap*/ ::\n cast (object);')['findings']
        self.assertEqual(rows[0]['line'], 2)

    def test_nested_ref_and_stored_alias(self):
        for text in ['ref(GIMP_OBJECT(obj))[fn]();', 'auto self = ref(obj); self[fn]();',
                     'auto self=(ref)(obj); self[fn]();', 'auto call=&ref; call(obj);',
                     'auto call=ref; call(obj);', 'auto call=&(ref); call(obj);']:
            self.assertTrue(inspect(text)['findings'])

    def test_private_placement_without_wrapper_name(self):
        self.assertTrue(inspect('void *p=G_TYPE_INSTANCE_GET_PRIVATE(obj,type,Impl); new(p) Impl(obj);')['findings'])

    def test_namespace_alias(self):
        self.assertTrue(inspect('namespace Old = GLib; auto x = Old::ref(obj);')['findings'])
        self.assertTrue(inspect('using namespace GLib; auto invoke = ref;')['findings'])

    def test_split_identifier_and_paste(self):
        self.assertEqual(inspect('NewG\\\nClass *x;')['findings'][0]['symbol'], 'NewGClass')
        self.assertEqual(inspect('#define MAKE NewG ## Class\n')['findings'][0]['symbol'], 'NewGClass')

    def test_pixel_functions(self):
        for name in ['tile_manager_ref', 'tile_manager_unref', 'pixel_region_init', 'pixel_regions_register']:
            self.assertTrue(inspect(name + '(x);')['findings'])

    def test_bridge_macros(self):
        for name in ['__DECLARE_GTK_CLASS__','__DECLARE_GTK_CAST__','__DECLARE_GTK_IFACE__','__DECLARE_GIMP_INTERFACE__']:
            self.assertTrue(inspect(name + '(X,Y);')['findings'])

    def test_includes(self):
        for header in ['base/glib-cxx-impl.hpp', 'gtk-cxx-utils.hpp', 'pdb-cxx-utils.hpp', 'base/tile-manager.h', 'pixel-region.h']:
            for directive in ['include','include_next']:
                for left,right in [('"','"'),('<','>')]:
                    with self.subTest(header=header,directive=directive,left=left):
                        self.assertTrue(inspect('#'+directive+' '+left+header+right+'\n')['findings'])

    def test_simple_header_macro(self):
        self.assertTrue(inspect('#define LEGACY_HEADER "base/glib-cxx-impl.hpp"\n#include LEGACY_HEADER\n')['findings'])
        self.assertTrue(inspect('#define ID(x) x\n#include ID("base/glib-cxx-impl.hpp")\n')['findings'])

    def test_comments_quoted_and_raw_strings(self):
        source = '// NewGClass\n/* ref(x)[fn] */\n"GLib::ref(\\\"value\\\") // not a comment";\n'
        source += 'u8R"tag(PixelRegion /* \" NewGClass\n#endif )tag";\n'
        source += "'x'; L\"Interface::cast\";\n"
        self.assertEqual(inspect(source)['findings'], [])

    def test_modern_helpers_and_placement_new(self):
        self.assertEqual(inspect('auto a = std::ref(x); new (storage) Callback(callback); auto ref = ObjectRef<T>::retain(x);')['findings'], [])

    def test_unknown_branches_all_scanned(self):
        source = '#if defined(WINDOWS)\nNewGClass a;\n#else\nPixelRegion b;\n#endif\n'
        self.assertEqual(len(inspect(source)['findings']), 2)

    def test_nested_zero_and_else(self):
        source = '#if 0\n#if UNKNOWN\nNewGClass a;\n#endif\n#else\nObjectRef<X> b;\n#endif\n'
        result = inspect(source)
        self.assertFalse(result['findings']); self.assertEqual(len(result['dormant_findings']),1)

    def test_elif_possible_and_impossible(self):
        source = '#if 0\nPixelRegion a;\n#elif UNKNOWN\nNewGClass b;\n#else\nIObject c;\n#endif\n'
        self.assertEqual(len(inspect(source)['findings']), 2)
        source = '#if 1\nObjectRef<X> a;\n#elif UNKNOWN\nNewGClass b;\n#else\nIObject c;\n#endif\n'
        self.assertFalse(inspect(source)['findings'])

    def test_multiline_directive_comment_cannot_hide_branch(self):
        source = '#if 0 /* comment\n continues */ || PLATFORM\nNewGClass x;\n#endif\n'
        self.assertEqual(len(inspect(source)['findings']),1)

    def test_elifdef_remains_possible(self):
        for directive in ['elifdef', 'elifndef']:
            self.assertTrue(inspect('#if 0\n#'+directive+' PLATFORM\nNewGClass x;\n#endif\n')['findings'])

    def test_malformed_source_is_not_a_pass(self):
        for text in ['/* unfinished', '"unfinished', 'R"x(unfinished', '#if 0\n', '#else\n', '#endif\n', '#if X\n#else\n#else\n#endif\n']:
            with self.subTest(text=text), self.assertRaises(ValueError): inspect(text)

    def test_data_calls_and_function_addresses(self):
        for verb in ['set','get','steal','replace','dup']:
            for kind in ['data','qdata']:
                name = 'g_object_'+verb+'_'+kind
                self.assertEqual(inspect(name+'(obj,key,value);')['data_sites'][0]['operation'],name)
                self.assertTrue(inspect('auto operation = '+name+';')['data_sites'][0]['indirect'])

    def test_pasted_data_call(self):
        self.assertEqual(inspect('#define PUT g_object_set_ ## data(obj,key,p)\n')['data_sites'][0]['operation'],'g_object_set_data')

    def test_literal_arguments_affect_fingerprint(self):
        one = inspect('g_object_set_data(obj,"key",p);')['data_sites'][0]['sha256']
        spaced = inspect('g_object_set_data ( obj, /* gap */ "key", p );')['data_sites'][0]['sha256']
        other = inspect('g_object_set_data(obj,"different",p);')['data_sites'][0]['sha256']
        self.assertEqual(one,spaced); self.assertNotEqual(one,other)


class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);(self.root/'app').mkdir()
        self.path=self.root/'app/example.cpp';self.path.write_text('g_object_set_data(obj,"key",p);')
        row=inspect(self.path.read_text())['data_sites'][0]
        self.policy={'syntax_exceptions':[],'dormant_exceptions':[],'reviews':{'native':{'paths':['app/example.cpp']}},
                     'data_sites':[{k:row[k] for k in ['path','operation','sha256']}|{'count':1,'reason':'Synthetic native control','provenance':'native'}]}

    def scan(self): return A.scan(self.root,self.policy)

    def test_exact_approved_site(self): self.assertEqual(self.scan()['status'],'PASS')

    def test_new_payload_rejected(self):
        self.path.write_text('g_object_set_data(obj,"key",new Impl);')
        self.assertEqual(self.scan()['status'],'FAIL')

    def test_duplicate_call_rejected(self):
        self.path.write_text(self.path.read_text()*2);self.assertEqual(self.scan()['status'],'FAIL')

    def test_removed_site_requires_review(self):
        self.path.write_text('');self.assertEqual(self.scan()['status'],'FAIL')

    def test_function_pointer_alias_rejected(self):
        self.path.write_text('auto put = g_object_set_data; put(obj,"key",p);')
        self.assertEqual(self.scan()['status'],'FAIL')

    def test_another_path_not_exempt(self):
        (self.root/'app/added.c').write_text(self.path.read_text());self.assertEqual(self.scan()['status'],'FAIL')

    def test_untracked_suffixes_scanned(self):
        for suffix in ['.cc','.cxx','.hh','.hpp','.hxx','.inc','.h++','.C','.h.in','.cpp.in']:
            file=self.root/('app/added'+suffix);file.write_text('NewGClass *x;')
            self.assertEqual(self.scan()['status'],'FAIL');file.unlink()

    def test_registered_test_source_cannot_hide(self):
        (self.root/'app/tests').mkdir();(self.root/'app/tests/actual.cpp').write_text('NewGClass *x;')
        self.assertEqual(self.scan()['status'],'PASS')
        self.policy['registered_cpp_paths']=['app/tests/actual.cpp']
        self.assertEqual(self.scan()['status'],'FAIL')

    def test_dormant_exception_and_reenable(self):
        self.path.write_text('g_object_set_data(obj,"key",p);\n#if 0\nTileManager *x;\n#endif\n')
        self.assertEqual(self.scan()['status'],'FAIL')
        rows=inspect(self.path.read_text())['dormant_findings']
        self.policy['dormant_exceptions']=[x|{'reason':'Pinned disabled control','provenance':'synthetic'} for x in rows]
        self.assertEqual(self.scan()['status'],'PASS')
        self.path.write_text(self.path.read_text().replace('#if 0','#if PLATFORM'))
        self.assertEqual(self.scan()['status'],'FAIL')

    def test_syntax_exception_not_general_ref_permission(self):
        self.path.write_text(self.path.read_text()+' Clone ref(owned());')
        row=inspect(self.path.read_text())['findings'][0]
        self.policy['syntax_exceptions']=[row|{'reason':'Typed local construction','provenance':'synthetic'}]
        self.assertEqual(self.scan()['status'],'PASS')
        self.path.write_text(self.path.read_text()+' auto self=ref(obj); self[fn]();')
        self.assertEqual(self.scan()['status'],'FAIL')

    def test_same_args_do_not_reuse_declaration_exception(self):
        self.path.write_text(self.path.read_text()+' Clone ref(owned());')
        row=inspect(self.path.read_text())['findings'][0]
        self.policy['syntax_exceptions']=[row|{'reason':'Typed local construction','provenance':'synthetic'}]
        self.assertEqual(self.scan()['status'],'PASS')
        self.path.write_text(self.path.read_text().replace('Clone ref(owned())','auto legacy=ref(owned())'))
        self.assertEqual(self.scan()['status'],'FAIL')

    def test_duplicate_policy_rejected(self):
        self.policy['data_sites']*=2
        with self.assertRaises(ValueError): self.scan()

    def test_unknown_provenance_rejected(self):
        self.policy['data_sites'][0]['provenance']='unreviewed'
        with self.assertRaises(ValueError): self.scan()

    def test_wrong_review_path_rejected(self):
        self.policy['reviews']['native']['paths']=['app/elsewhere.c']
        with self.assertRaises(ValueError): self.scan()

    def test_missing_upstream_provenance_rejected(self):
        self.policy['data_sites'][0]['provenance']='upstream'
        with self.assertRaises(ValueError): self.scan()

    def test_symlink_requires_scope_review(self):
        (self.root/'app/alias.cpp').symlink_to(self.path)
        with self.assertRaises(ValueError): self.scan()

    def test_malformed_source_fails_scan(self):
        self.path.write_text('#if 0\nNewGClass *x;');self.assertEqual(self.scan()['status'],'FAIL')

    def test_empty_scope_cannot_pass(self):
        self.path.unlink();self.policy['data_sites']=[]
        self.assertEqual(self.scan()['status'],'FAIL')


class NativeWidgetMetadataTests(unittest.TestCase):
    """Pin the two 08.018 native metadata reviews without general qdata access."""
    SITES = (
        ('native-editor-help', 'app/widgets/gimpeditor-cxx.cpp',
         'g_object_set_qdata_full',
         '41f19886d64f6911801df4adead4965ff28daa68f973131a5646e7596ee944a3',
         'g_object_set_qdata_full (arrow.get (), GIMP_HELP_ID, g_strdup (help_id), g_free);'),
        ('native-radio-button', 'app/widgets/gimppaintercompactoptions.cpp',
         'g_object_get_data',
         '5aa62660106766da81a0f878ff53a3627f514343ed8e82c15687fc478ad74bfb',
         'g_object_get_data (G_OBJECT (widget), "radio-button");'),
    )

    def setUp(self):
        root = Path(__file__).resolve().parents[2]
        full = json.loads((root / A.POLICY).read_text())
        names = {site[0] for site in self.SITES}
        self.policy = {
            'syntax_exceptions': [], 'dormant_exceptions': [],
            'reviews': {name: full['reviews'][name] for name in names},
            'data_sites': [row for row in full['data_sites'] if row['provenance'] in names],
        }
        self.calls = {site[1]: site[4] for site in self.SITES}

    def scan(self, calls=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name, call in (self.calls if calls is None else calls).items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(call + '\n')
            return A.scan(root, self.policy)

    def test_policy_pins_exact_native_metadata_sites(self):
        self.assertEqual(len(self.policy['data_sites']), 2)
        for name, path, operation, sha256, _ in self.SITES:
            rows = [row for row in self.policy['data_sites'] if row['provenance'] == name]
            self.assertEqual(len(rows), 1)
            row = rows[0]
            self.assertEqual((row['path'], row['operation'], row['sha256'], row['count']),
                             (path, operation, sha256, 1))
            self.assertEqual(self.policy['reviews'][name]['paths'], [path])
            self.assertTrue(row['reason'])
            self.assertTrue(self.policy['reviews'][name]['reason'])

    def test_exact_native_metadata_calls(self):
        self.assertEqual(self.scan()['status'], 'PASS')

    def test_unknown_qdata_site_rejected(self):
        for _, path, _, _, _ in self.SITES:
            with self.subTest(path=path):
                calls = dict(self.calls)
                calls[path] += '\ng_object_set_qdata_full (object, impl_key, new Impl, destroy_impl);'
                result = self.scan(calls)
                self.assertEqual(result['status'], 'FAIL')
                self.assertTrue(any('unaudited object data site' in error for error in result['errors']))

    def test_mutated_native_metadata_calls_rejected(self):
        changes = (
            (0, 'arrow.get ()', 'primary.get ()'),
            (0, 'GIMP_HELP_ID', 'impl_key'),
            (0, 'g_strdup (help_id)', 'new Impl'),
            (0, 'g_free', 'destroy_impl'),
            (1, 'G_OBJECT (widget)', 'G_OBJECT (other)'),
            (1, '"radio-button"', '"impl"'),
        )
        for index, old, new in changes:
            with self.subTest(index=index, old=old):
                calls = dict(self.calls)
                path = self.SITES[index][1]
                calls[path] = calls[path].replace(old, new)
                result = self.scan(calls)
                self.assertEqual(result['status'], 'FAIL')
                self.assertTrue(any('unaudited object data site' in error for error in result['errors']))
                self.assertTrue(any('stale or changed object data review' in error for error in result['errors']))

    def test_duplicate_native_metadata_calls_rejected(self):
        for _, path, _, _, _ in self.SITES:
            with self.subTest(path=path):
                calls = dict(self.calls)
                calls[path] *= 2
                self.assertEqual(self.scan(calls)['status'], 'FAIL')

    def test_native_metadata_source_path_drift_rejected(self):
        for _, path, _, _, _ in self.SITES:
            with self.subTest(path=path):
                calls = dict(self.calls)
                calls[path.replace('.cpp', '-moved.cpp')] = calls.pop(path)
                result = self.scan(calls)
                self.assertEqual(result['status'], 'FAIL')
                self.assertTrue(any('unaudited object data site' in error for error in result['errors']))
                self.assertTrue(any('stale or changed object data review' in error for error in result['errors']))

    def test_duplicate_native_metadata_policy_rejected(self):
        for row in self.policy['data_sites'][:]:
            with self.subTest(path=row['path']):
                self.policy['data_sites'].append(dict(row))
                with self.assertRaisesRegex(ValueError, 'duplicate data-site policy entry'):
                    self.scan()
                self.policy['data_sites'].pop()

    def test_unknown_native_metadata_provenance_rejected(self):
        for row in self.policy['data_sites']:
            with self.subTest(path=row['path']):
                previous = row['provenance']
                row['provenance'] = 'unreviewed-native-metadata'
                with self.assertRaisesRegex(ValueError, 'unknown explicit data-site review'):
                    self.scan()
                row['provenance'] = previous

    def test_native_metadata_review_path_drift_rejected(self):
        for name, review in self.policy['reviews'].items():
            with self.subTest(review=name):
                previous = review['paths']
                review['paths'] = ['app/widgets/unreviewed.cpp']
                with self.assertRaisesRegex(ValueError, 'data-site review does not cover this source path'):
                    self.scan()
                review['paths'] = previous


class NativeToolbarMetadataTests(NativeWidgetMetadataTests):
    """Exact 08.018 toolbar reviews; no general object-data permission."""
    SITES = (
        ('native-toolbar-factory', 'app/tools/gimp-tools.c',
         'g_object_get_data',
         '210be2a0c318930adf198948b10b0955b844eb99d22755b2e9d9bd67e0e51213',
         'g_object_get_data ( G_OBJECT ( options -> tool_info ) , "gimp-tool-options-gui-func" );'),
        ('native-toolbar-factory', 'app/tools/gimp-tools.c',
         'g_object_set_data',
         'c2cba03c6ffa32e15986fb629717ddc4475448d688996c8417505441f660d017',
         'g_object_set_data ( G_OBJECT ( tool_info ) , "gimp-tool-options-gui-horizontal-func" , gimp_tools_horizontal_options_gui );'),
        ('native-toolbar-popup-child', 'app/tools/gimppainterpainttoolbar.c',
         'g_object_set_data',
         '4e91be29b4311fa7a178a5d750ced1b61fab252fa20ec15dbb390e8a56ed3876',
         'g_object_set_data ( G_OBJECT ( button ) , "gimp-painter-toolbar-popup" , contents );'),
        ('native-toolbar-brush-property', 'app/tools/gimppainterpainttoolbar.c',
         'g_object_get_data',
         '1ce683ec5b0aa17313fc28b0002bc6a49908c00dfec6dd83de8556206ca93e2d',
         'g_object_get_data ( G_OBJECT ( button ) , "brush-property" );'),
        ('native-toolbar-brush-property', 'app/tools/gimppainterpainttoolbar.c',
         'g_object_set_data_full',
         '62ccd8a6750d1542b5719cdf1bedbedf5ade5e020661643a7358a59437578194',
         'g_object_set_data_full ( G_OBJECT ( reset ) , "brush-property" , g_strdup ( property ) , g_free );'),
        ('native-toolbar-mode-child', 'app/tools/gimppainterpainttoolbar.c',
         'g_object_set_data',
         '835f168c556b3fd36e25db438a3c9821085c5ac785127484aaac3117ad980484',
         'g_object_set_data ( G_OBJECT ( toolbar ) , "gimp-paint-options-gui-paint-mode-box" , mode );'),
        ('native-toolbar-options', 'app/tools/gimppainterpainttoolbar.c',
         'g_object_set_data_full',
         '9c2431cab9b0f75b10ca462b7194ce902746ecd4cbd9adc53b5e9a02be2b2d26',
         'g_object_set_data_full ( G_OBJECT ( toolbar ) , "gimp-painter-toolbar-options" , g_object_ref ( tool_options ) , g_object_unref );'),
        ('native-toolbar-scalar', 'app/tools/gimppaintertooltoolbar.c',
         'g_object_get_data',
         '6a89ec623f3dddc2bf1d94c886202a57e410c443d8094bafaacf699efd4c2326',
         'g_object_get_data ( G_OBJECT ( row ) , "painter-fixed-rule" );'),
        ('native-toolbar-scalar', 'app/tools/gimppaintertooltoolbar.c',
         'g_object_set_data',
         'c652c481d5ed8153ae7a45b19dabc3f0a5ffb7a36cad495c9f5534d4d19c721f',
         'g_object_set_data ( G_OBJECT ( row ) , "painter-fixed-rule" , GINT_TO_POINTER ( rule ) );'),
        ('native-toolbar-scalar', 'app/tools/gimppaintertooltoolbar.c',
         'g_object_get_data',
         'feaebce3f59b9b359a07e1b6056055b222467d67cd5c531a1374b82005e6f20e',
         'g_object_get_data ( G_OBJECT ( combo ) , "painter-gradient-metric" );'),
        ('native-toolbar-scalar', 'app/tools/gimppaintertooltoolbar.c',
         'g_object_set_data',
         '0a7d7b4c87e395b33d53b7a7939cb1df5d357765ef80e0536e084edf6bd0937d',
         'g_object_set_data ( G_OBJECT ( combo ) , "painter-gradient-metric" , GINT_TO_POINTER ( TRUE ) );'),
        ('native-toolbar-options', 'app/tools/gimppaintertooltoolbar.c',
         'g_object_set_data_full',
         '38c5e9ed95972e0e6a48acb643e7bcf30379c9d467cdd408a8b07d5f0cd4d24c',
         'g_object_set_data_full ( G_OBJECT ( box ) , "gimp-painter-toolbar-options" , g_object_ref ( options ) , g_object_unref );'),
        ('native-toolbar-options', 'app/tools/gimppaintertransformtoolbar.c',
         'g_object_set_data_full',
         '8595250acd6e0be229fd3c643fb517722893f41ee85bad33b41cae487fc91056',
         'g_object_set_data_full ( G_OBJECT ( root ) , "gimp-painter-toolbar-options" , g_object_ref ( options ) , g_object_unref );'),
        ('native-toolbar-factory', 'app/widgets/gimpwidgets-utils.c',
         'g_object_get_data',
         'f70a0826413b5d47f162fce9407395f331ab1db34f87c3d45b0977114eb9185e',
         'g_object_get_data ( G_OBJECT ( tool_info ) , "gimp-tool-options-gui-horizontal-func" );'),
    )

    def setUp(self):
        super().setUp()
        self.calls = {}
        for _, path, _, _, call in self.SITES:
            self.calls[path] = self.calls.get(path, '') + call + '\n'

    def test_policy_pins_exact_native_metadata_sites(self):
        self.assertEqual(len(self.policy['data_sites']), 14)
        for name, path, operation, sha256, _ in self.SITES:
            rows = [row for row in self.policy['data_sites']
                    if (row['path'], row['operation'], row['sha256']) ==
                       (path, operation, sha256)]
            self.assertEqual(len(rows), 1)
            self.assertEqual((rows[0]['count'], rows[0]['provenance']), (1, name))
            self.assertIn(path, self.policy['reviews'][name]['paths'])
            self.assertTrue(rows[0]['reason'])
            self.assertTrue(self.policy['reviews'][name]['reason'])

    def test_mutated_native_metadata_calls_rejected(self):
        for _, path, operation, _, call in self.SITES:
            mutations = [call.replace('"', '"changed-', 1)]
            if operation == 'g_object_set_data_full':
                mutations.append(call.replace('g_free', 'destroy_impl')
                                 if 'g_free' in call else
                                 call.replace('g_object_unref', 'destroy_impl'))
                mutations.append(call.replace('g_object_ref ( options )', 'new Impl')
                                 .replace('g_object_ref ( tool_options )', 'new Impl')
                                 .replace('g_strdup ( property )', 'new Impl'))
            for changed in mutations:
                with self.subTest(path=path, changed=changed):
                    self.assertNotEqual(call, changed)
                    calls = dict(self.calls)
                    calls[path] = calls[path].replace(call, changed)
                    result = self.scan(calls)
                    self.assertEqual(result['status'], 'FAIL')
                    self.assertTrue(any('unaudited object data site' in e for e in result['errors']))
                    self.assertTrue(any('stale or changed object data review' in e for e in result['errors']))

    def test_duplicate_native_metadata_calls_rejected(self):
        for _, path, _, _, call in self.SITES:
            with self.subTest(path=path, call=call):
                calls = dict(self.calls)
                calls[path] += call
                self.assertEqual(self.scan(calls)['status'], 'FAIL')

    def test_native_metadata_source_path_drift_rejected(self):
        for path in self.calls:
            with self.subTest(path=path):
                calls = dict(self.calls)
                calls[path + '.inc'] = calls.pop(path)
                self.assertEqual(self.scan(calls)['status'], 'FAIL')

    def test_obsolete_canvas_review_removed(self):
        root = Path(__file__).resolve().parents[2]
        full = json.loads((root / A.POLICY).read_text())
        self.assertNotIn('native-color-button', full['reviews'])
        self.assertFalse(any(row['provenance'] == 'native-color-button'
                             for row in full['data_sites']))
        path = 'app/display/gimppaintercanvasui.cpp'
        self.assertEqual(A.inspect_source((root / path).read_text(), path)['data_sites'], [])


if __name__ == '__main__':
    unittest.main(verbosity=2)
