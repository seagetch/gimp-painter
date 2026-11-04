"""C39: real headless GTK3 theme invalidation ordering, not GIMP acceptance.

Only a standalone GtkIconTheme and temporary fixtures are used. A Python
sentinel models construction; it is deliberately not a widget/layout assertion.
The production GLib main-loop helper is extracted and compiled unchanged.
"""
import ctypes as C
import ctypes.util
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'app/tests/tests.c'


def function(source, name):
    """Extract a complete C function, including its return type."""
    match = re.search(r'(?m)^(?:static )?(?:void|gboolean)\n' + name + r' \(', source)
    if not match:
        raise AssertionError('Missing function: ' + name)
    start = source.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def bind(lib, name, result, *args):
    fn = getattr(lib, name)
    fn.restype, fn.argtypes = result, args
    return fn


def probe(mode=None):
    """Run in a fresh process so default-context sources cannot leak between tests."""
    gtk = C.CDLL(ctypes.util.find_library('gtk-3') or 'libgtk-3.so.0')
    glib_name = ctypes.util.find_library('glib-2.0') or 'libglib-2.0.so.0'
    obj = C.CDLL(ctypes.util.find_library('gobject-2.0') or 'libgobject-2.0.so.0')
    ptr = C.c_void_p
    new = bind(gtk, 'gtk_icon_theme_new', ptr)
    custom = bind(gtk, 'gtk_icon_theme_set_custom_theme', None, ptr, C.c_char_p)
    paths = bind(gtk, 'gtk_icon_theme_set_search_path', None, ptr, C.POINTER(C.c_char_p), C.c_int)
    prepend = bind(gtk, 'gtk_icon_theme_prepend_search_path', None, ptr, C.c_char_p)
    has = bind(gtk, 'gtk_icon_theme_has_icon', C.c_int, ptr, C.c_char_p)
    callback_type = C.CFUNCTYPE(None, ptr, ptr)
    connect = bind(obj, 'g_signal_connect_data', C.c_ulong, ptr, C.c_char_p,
                   callback_type, ptr, ptr, C.c_int)
    disconnect = bind(obj, 'g_signal_handler_disconnect', None, ptr, C.c_ulong)
    unref = bind(obj, 'g_object_unref', None, ptr)
    source = SOURCE.read_text()
    abi = '''#include <assert.h>
#include <stdint.h>
typedef int64_t gint64;
typedef int gboolean;
typedef void *gpointer;
typedef unsigned int guint;
typedef struct _GMainLoop GMainLoop;
typedef gboolean (*GSourceFunc)(gpointer);
#define NULL ((void *)0)
#define FALSE 0
#define TRUE 1
#define G_SOURCE_REMOVE 0
#define G_PRIORITY_HIGH (-100)
#define G_TIME_SPAN_SECOND 1000000
#define g_assert_true(value) assert(value)
#define g_assert_false(value) assert(!(value))
#define g_assert_cmpint(a, op, b) assert((a) op (b))
extern gint64 g_get_monotonic_time(void);
extern guint g_timeout_add_full(int, guint, GSourceFunc, gpointer, void (*)(gpointer));
extern gboolean g_source_remove(guint);
extern GMainLoop *g_main_loop_new(void *, gboolean);
extern void g_main_loop_quit(GMainLoop *);
extern void g_main_loop_run(GMainLoop *);
extern void g_main_loop_unref(GMainLoop *);
extern guint g_idle_add(GSourceFunc, gpointer);
'''
    results = {}
    with tempfile.TemporaryDirectory(prefix='c39-theme-') as directory:
        base = Path(directory)
        cfile, shared = base / 'helper.c', base / 'helper.so'
        setup_start = source.index('typedef struct', source.index('/* Test-only setup settlement.'))
        setup_end = source.index('static void\ngimp_init_icon_theme_for_testing', setup_start)
        # Compile the actual bounded helper and callbacks, not a reimplementation.
        cfile.write_text(abi + source[setup_start:setup_end] +
                        function(source, 'gimp_tests_quit_mainloop') + '\n' +
                        function(source, 'gimp_test_run_mainloop_until_idle') +
                        '\nvoid run_setup(void) { gimp_test_finish_icon_theme_setup(); }\n')
        subprocess.run(['cc', '-shared', '-fPIC', '-Wall', '-Wextra', '-Werror',
                        str(cfile), '-o', str(shared), '-l:' + glib_name], check=True)
        helper = C.CDLL(str(shared))
        drain = bind(helper, 'gimp_test_run_mainloop_until_idle', None)
        settle = bind(helper, 'run_setup', None)
        if mode:
            glib = C.CDLL(glib_name)
            idle_type = C.CFUNCTYPE(C.c_int, ptr)
            add_idle = bind(glib, 'g_idle_add_full', C.c_uint, C.c_int, idle_type, ptr, ptr)
            @idle_type
            def interfere(_data):
                if mode == 'starved':
                    return 1
                # Same-priority ready idle is in this dispatch batch, so the
                # elapsed assertion must catch the nonpreemptive overrun.
                import time
                time.sleep(3.1)
                return 0
            add_idle(0 if mode == 'starved' else 200, interfere, None, None)
            settle()
            raise AssertionError('Setup unexpectedly accepted ' + mode)
        icons = base / 'icons'
        theme_dir = icons / 'c39-fixture'
        fixture = theme_dir / '16x16/actions'
        fixture.mkdir(parents=True)
        (theme_dir / 'index.theme').write_text('''[Icon Theme]
Name=C39 isolated fixture
Comment=Temporary regression fixture
Directories=16x16/actions

[16x16/actions]
Size=16
Context=Actions
Type=Fixed
''')
        (fixture / 'c39-probe.svg').write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16">'
            '<rect width="16" height="16" fill="black"/></svg>')

        for case in ('old', 'new', 'coalesced', 'no_pending'):
            theme = new()
            assert theme
            observed = []
            state = {'constructed': False}
            @callback_type
            def changed(_theme, _data):
                observed.append(state['constructed'])
            handler = connect(theme, b'changed', changed, None, None, 0)
            try:
                paths(theme, (C.c_char_p * 1)(os.fsencode(icons)), 1)
                custom(theme, b'c39-fixture')
                assert has(theme, b'c39-probe'), 'Fixture must load a valid real theme'
                drain()
                observed.clear()
                if case != 'no_pending':
                    prepend(theme, os.fsencode(base / ('extra-' + case)))
                    assert observed == [], 'Invalidation must be asynchronous'
                    if case == 'coalesced':
                        # Reload before mutating again while the first changed idle
                        # is pending. GTK must coalesce into the same notification.
                        assert has(theme, b'c39-probe')
                        prepend(theme, os.fsencode(base / 'extra-coalesced-second'))
                        assert observed == []
                if case != 'old':
                    settle()
                state['constructed'] = True
                drain()
                expected = [True] if case == 'old' else ([] if case == 'no_pending' else [False])
                assert observed == expected, (case, observed, expected)
                before = list(observed)
                # A later genuine mutation remains observable: no suppression,
                # disconnection, freeze, or changed-signal replacement is involved.
                assert has(theme, b'c39-probe')
                prepend(theme, os.fsencode(base / ('later-' + case)))
                assert observed == before
                drain()
                assert observed == before + [True], (case, observed)
                results[case] = {'startup': before, 'after_later_mutation': list(observed)}
            finally:
                disconnect(theme, handler)
                unref(theme)
                drain()
    return results


class SourceContracts(unittest.TestCase):
    def test_activation_settles_theme_before_initialize_and_restore(self):
        body = function(SOURCE.read_text(), 'gimp_test_app_activate_callback')
        # Allow explanatory comments, not intervening C statements.
        code = re.sub(r'/\*.*?\*/', '', body, flags=re.S)
        self.assertRegex(code, r'gimp_init_icon_theme_for_testing \(\);\s*'
                         r'gimp_test_finish_icon_theme_setup \(\);')
        settle = code.index('gimp_test_finish_icon_theme_setup ();')
        self.assertLess(code.index('gui_init ('), settle)
        self.assertLess(settle, code.index('gimp_initialize ('))
        self.assertLess(settle, code.index('gimp_restore ('))
        self.assertLess(settle, code.index('g_test_run ()'))
        self.assertEqual(code.count('gimp_test_finish_icon_theme_setup ();'), 1)
        theme = function(SOURCE.read_text(), 'gimp_init_icon_theme_for_testing')
        self.assertNotIn('gimp_test_finish_icon_theme_setup', theme)
        self.assertIn('gtk_icon_theme_prepend_search_path (gtk_icon_theme_get_default (),', theme)
        self.assertNotRegex(code, r'g_signal_(?:stop_emission|handler_block)|g_test_skip')

    def test_exact_default_idle_helper_semantics_preserved(self):
        source = SOURCE.read_text()
        self.assertEqual(function(source, 'gimp_test_run_mainloop_until_idle'), '''void
gimp_test_run_mainloop_until_idle (void)
{
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);

  g_idle_add ((GSourceFunc) gimp_tests_quit_mainloop, loop);

  g_main_loop_run (loop);

  g_main_loop_unref (loop);
}''')
        self.assertEqual(function(source, 'gimp_tests_quit_mainloop'), '''static gboolean
gimp_tests_quit_mainloop (GMainLoop *loop)
{
  g_main_loop_quit (loop);

  return FALSE;
}''')

    def test_original_popup_deadline_and_private_wait_unchanged(self):
        for path, digest in (
            ('app/tests/test-painter-transform-popup.inc',
             '39497319586c30f92cd6d501d84352ca54b7d5a8b49e75a00694f391a9901c16'),
            ('app/tests/test-painter-brush-thumbnail-popup.inc',
             '6f828b0b239e4714ed29475bef38a7c2021cd6fa6d1c0a76af75d493cb639a10')):
            self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), digest, path)

    def test_all_26_common_protected_files_unchanged(self):
        pins = json.loads((ROOT / 'tools/brush-ci/pins.json').read_text())
        protected = pins['common_protected_file_sha256']
        self.assertEqual(len(protected), 26)
        for path, digest in protected.items():
            self.assertEqual(hashlib.sha256((ROOT / path).read_bytes()).hexdigest(), digest, path)


class ThemeSetup(unittest.TestCase):
    def test_bounded_setup_rejects_starvation_and_callback_overrun(self):
        for mode, assertion in (('starved', 'setup.ready'), ('overrun', 'elapsed')):
            with self.subTest(mode=mode):
                result = subprocess.run([sys.executable, str(Path(__file__).resolve()),
                                         '--probe', mode], capture_output=True,
                                        text=True, timeout=10)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(assertion, result.stderr)

    def test_real_gtk_pending_coalesced_and_absent_invalidations(self):
        env = dict(os.environ)
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        result = subprocess.run([sys.executable, str(Path(__file__).resolve()), '--probe'],
                                env=env, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, '', 'Unexpected native warnings: ' + result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data['old']['startup'], [True])
        self.assertEqual(data['new']['startup'], [False])
        self.assertEqual(data['coalesced']['startup'], [False])
        self.assertEqual(data['no_pending']['startup'], [])
        for value in data.values():
            self.assertEqual(value['after_later_mutation'], value['startup'] + [True])



if __name__ == '__main__':
    if sys.argv[1:2] == ['--probe']:
        # Deliberate assertion failures must not leave core dumps anywhere.
        import resource
        resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
        print(json.dumps(probe(sys.argv[2] if len(sys.argv) > 2 else None), sort_keys=True))
    else:
        unittest.main()
