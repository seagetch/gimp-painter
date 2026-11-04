"""Headless real-GObject property/lifetime tests; synthetic GTK, not GUI proof."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
HERE = Path(__file__).resolve().parent

class IconTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        path = Path(cls.temp.name)
        reference = HERE / 'image.inc'
        if not reference.exists():
            reference = HERE.parent / 'c42-recovery/unpacked/source/tools/brush-ci/readiness-owner/image.inc'
        source = '\n'.join(p.read_text() for p in [HERE/'synthetic-icon-prefix.c', reference, HERE/'icon.inc', HERE/'synthetic-icon-suffix.c'])
        (path/'test.c').write_text(source)
        cls.binary = path/'test'
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(cls.binary),'-l:libgobject-2.0.so.0','-l:libglib-2.0.so.0'],check=True)
    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()
    def run_case(self, mode, flags):
        result = subprocess.run([str(self.binary), mode],capture_output=True,text=True,check=True,timeout=10)
        self.assertEqual(result.stderr,'')
        rows = [r for r in result.stdout.splitlines() if r.startswith('ICON_DIAG')]
        self.assertEqual(rows[-1], 'ICON_DIAG complete')
        for row in rows[:-1]: self.assertRegex(row,r'^ICON_DIAG (summary|group|overflow)( -?[0-9]+)+$')
        self.assertNotIn('SECRET',result.stdout)
        summary = list(map(int, rows[0].split()[2:])); self.assertEqual(len(summary),9)
        self.assertEqual(summary[2],flags)
        groups = [list(map(int,r.split()[2:])) for r in rows[1:-2]]
        overflow = list(map(int,rows[-2].split()[2:])); self.assertEqual(len(overflow),6)
        self.assertEqual(len(groups),summary[4])
        for i,g in enumerate(groups,1):
            self.assertEqual(len(g),20); self.assertEqual(g[0],i)
            self.assertEqual(g[14],g[15]+g[16]); self.assertLessEqual(g[17],g[14])
            if g[1] == 6: self.assertEqual(g[2:13],[0,0,0,-1,-1,0,0,0,0,0,0])
        if mode != 'counter':
            for n in range(3): self.assertEqual(summary[5+n],sum(g[14+n] for g in groups)+overflow[n])
        return summary,groups,overflow
    def test_equality_configs_untracked(self):
        s,g,_=self.run_case('normal',0)
        self.assertEqual(s[3:7],[2,6,7,7]); self.assertEqual(g[0][14],2)
        self.assertEqual(g[-1][13],0)
    def test_string_bounds_unknowns(self):
        _,g,_=self.run_case('strings',2)
        self.assertEqual([x[1] for x in g],[1,4,2,3,1])
    def test_dictionary_and_config_caps_preserve_existing_keys(self):
        s,g,_=self.run_case('dictionary',12); self.assertEqual(s[3],256); self.assertEqual(g[0][14],2)
        _,g,o=self.run_case('groups',8); self.assertEqual(len(g),128); self.assertEqual(o[:2],[2,2]); self.assertEqual(g[0][14],2)
    def test_nested_inclusive_and_destroy_no_postread(self):
        self.run_case('nested',0); self.run_case('destroy',0)
    def test_failure_flags(self):
        for mode,flags in [('clock',32),('duration',16),('counter',80),('unmatched',64),('inflight',64),('invalid',128),('subtype',128)]:
            with self.subTest(mode=mode): self.run_case(mode,flags)
    def test_setup_fails_closed(self):
        self.run_case("setup-disabled",1); self.run_case("setup-property",1)
    def test_static_safety(self):
        source=(HERE/'icon.inc').read_text()
        hot=source.split('static guint\nicon_name_id',1)[1].split('static void\nicon_print_metric',1)[0]
        self.assertNotRegex(hot,r'\b(malloc|calloc|realloc|g_new|g_strdup|g_object_get|g_object_get_property|gtk_icon_theme_|gtk_widget_get_style_context|printf|strlen|strcmp)\s*\(')
        exit=hot.split('icon_observe_exit (IconCall call)',1)[1]
        self.assertNotRegex(exit,r'\b(gtk_|g_object_|G_OBJECT)')
        token=re.search(r'typedef struct \{ guint group;.*?\} IconCall;',source).group()
        self.assertNotIn('*',token)

if __name__ == '__main__': unittest.main()
