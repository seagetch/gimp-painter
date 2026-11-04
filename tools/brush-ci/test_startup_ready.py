"""C37 exact C35 restoration + original deadline proof; no native acceptance."""
import hashlib
from pathlib import Path
import subprocess
import tempfile
import unittest
HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
SOURCE=ROOT/'app/tests/test-painter-brush-thumbnail-popup.inc'
class StartupReady(unittest.TestCase):
    def test_c35_source_restored_exactly(self):
        # C36 introduced a separate startup-host assumption. The whole C35
        # include is restored, not a zero-host skip or a new timing allowance.
        self.assertEqual(hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
                         "6f828b0b239e4714ed29475bef38a7c2021cd6fa6d1c0a76af75d493cb639a10")
    def test_no_startup_host_assumption_or_bypass(self):
        source=SOURCE.read_text()
        for removed in ('brush_popup_require_initial_main_window_ready',
                        'BRUSH STARTUP FRAME PRECONDITION', 'BrushPopupStartup'):
            self.assertNotIn(removed,source)
        case=source.split('brush_popup_private_target_release (void)\n{\n',1)[1]
        case=case[:case.index('\ntypedef struct')]
        self.assertEqual(hashlib.sha256(case.encode()).hexdigest(),
                         '0f6d731a55ec631b684b2a991b7256d2025c1c99041b9d7d1afcd42c4b1ad612')
        for forbidden in ('gimp_get_image_windows', 'g_test_skip', 'return;',
                          '10 * G_TIME_SPAN_SECOND'):
            self.assertNotIn(forbidden,case)
    def test_original_wait_loop_late_target_still_fails(self):
        source=(ROOT/'app/tests/test-painter-transform-popup.inc').read_text()
        start=source.index('  gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;')
        loop=source[start:source.index('  gint64 elapsed =',start)]
        prefix="""#include <stdint.h>
#include <stddef.h>
#include <assert.h>
typedef int64_t gint64;
#define G_TIME_SPAN_SECOND 1000000
#define FALSE 0
#define MAX(a,b) ((a)>(b)?(a):(b))
static gint64 now;
static gint64 g_get_monotonic_time(void){return now;}
static void g_main_context_iteration(void *p,int block){(void)p;(void)block;now+=3100000;}
static void g_usleep(int n){now+=n;}
int main(void){struct {unsigned stable;} layout={0};unsigned iterations=0;gint64 longest_iteration=0;
"""
        suffix='  assert(iterations==1 && longest_iteration==3100000);\n  return layout.stable >= 3 ? 0 : 2;\n}\n'
        with tempfile.TemporaryDirectory() as tmp:
            c=Path(tmp)/'negative.c';exe=Path(tmp)/'negative';c.write_text(prefix+loop+suffix)
            subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(c),'-o',str(exe)],check=True)
            result=subprocess.run([str(exe)],check=False)
            self.assertEqual(result.returncode,2)
    def test_original_private_first_wait_preserved(self):
        source=SOURCE.read_text();opening=source.split('brush_popup_open (BrushPopupFixture *f, gboolean physical)\n{',1)[1]
        self.assertIn('transform_popup_wait_stable_geometry (f->button);',opening.split('TransformPopupPosition',1)[0])
        original=(ROOT/'app/tests/test-painter-transform-popup.inc').read_bytes()
        self.assertEqual(hashlib.sha256(original).hexdigest(),'39497319586c30f92cd6d501d84352ca54b7d5a8b49e75a00694f391a9901c16')
        self.assertIn(b'gint64 deadline = g_get_monotonic_time () + 3 * G_TIME_SPAN_SECOND;',original)
        self.assertIn(b'g_assert_cmpuint (layout.stable, >=, 3);',original)
if __name__=='__main__':unittest.main()
