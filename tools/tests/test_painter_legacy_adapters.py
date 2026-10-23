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


if __name__ == '__main__':
    unittest.main(verbosity=2)
