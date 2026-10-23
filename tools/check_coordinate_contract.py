#!/usr/bin/env python3
"""Check 09.002 provenance and execute extracted coordinate code and real GEGL.

This is a correspondence/contract check, not a migrated DrawableFeature test.
"""
import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'migration/contracts/coordinate-contract.json'
I32_MIN, I32_MAX = -(2 ** 31), 2 ** 31 - 1


def require(condition, message):
    if not condition:
        raise ValueError(message)


def run(command, **kwargs):
    return subprocess.check_output(command, text=True, **kwargs)


def extract_function(source, name):
    match = re.search(r'^' + re.escape(name) + r'\s*\([^;]*?\)\s*\{', source, re.M)
    require(match is not None, 'Missing function: ' + name)
    start = source.rfind('\n', 0, match.start() - 1) + 1
    position = source.index('{', match.start())
    depth, end = 1, position + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end] + '\n'


def checked_rect(rect):
    """Safety model for future adapters, NOT an implementation used by GIMP."""
    require(len(rect) == 4 and all(type(v) is int for v in rect), 'Integer rectangle required')
    x, y, width, height = rect
    require(all(I32_MIN <= v <= I32_MAX for v in rect), 'Rectangle outside gint range')
    require(width >= 0 and height >= 0, 'Negative rectangle size')
    require(x + width <= I32_MAX and y + height <= I32_MAX, 'Endpoint outside gint range')
    return x, y, width, height


def intersection(a, b):
    ax, ay, aw, ah = checked_rect(a)
    bx, by, bw, bh = checked_rect(b)
    if not aw or not ah or not bw or not bh:
        return None
    x, y = max(ax, bx), max(ay, by)
    right, bottom = min(ax + aw, bx + bw), min(ay + ah, by + bh)
    return [x, y, right - x, bottom - y] if right > x and bottom > y else None


def validate(manifest):
    require(manifest['task'] == '09.002', 'Wrong task')
    require(manifest['legacy_commit'] == 'afa43fae3e920210146abed514f136fd49f671b5', 'Wrong legacy revision')
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        ledger = {r['work_id']: r for r in csv.DictReader(stream, delimiter='\t') if r['wbs_task'] == '09.002'}
    duties = manifest['duties']
    require(len(duties) == 13 and len({d['work_id'] for d in duties}) == 13, 'Duplicate or missing duty')
    require(set(ledger) == {d['work_id'] for d in duties}, 'Duty coverage differs from source ledger')
    sources = {}
    for record in duties + manifest['legacy_context']:
        if 'work_id' in record:
            for key in ('source_id', 'path', 'source_blob', 'source_range', 'source_sha256'):
                require(record[key] == ledger[record['work_id']][key], 'Wrong source identity: ' + key)
            require(bool(record['mapping']) and record['check'] in ('dispatch', 'temp-window', 'scalar-only'), 'Unmapped duty')
        path = record['path']
        source = run(['git', 'show', manifest['legacy_commit'] + ':' + path], cwd=ROOT)
        require(hashlib.sha256(source.encode()).hexdigest() == record['blob_sha256'], 'Wrong source bytes: ' + path)
        require(run(['git', 'rev-parse', manifest['legacy_commit'] + ':' + path], cwd=ROOT).strip() == record['source_blob'], 'Wrong source blob')
        sources[path] = source
    required_context = {'app/base/pixel-region.c', 'app/base/pixel-region.h', 'app/base/base-types.h',
                        'app/base/pixel-processor.h', 'app/paint/gimpmypaintcore-drawablefeature.hpp',
                        'app/display/gimpdisplayshell-transform.c', 'app/display/gimpdisplayshell.h',
                        'libgimpmath/gimpmath.h'}
    require(len(manifest['legacy_context']) == len(required_context) and
            {r['path'] for r in manifest['legacy_context']} == required_context, 'Wrong context source set')
    require(set(manifest['spaces']) == {'image', 'layer', 'buffer', 'screen'}, 'Missing coordinate space')
    require(all(space['axes'] == ['right', 'down'] for space in manifest['spaces'].values()), 'Wrong coordinate axes')
    require(manifest['spaces']['layer']['to_image'] == 'image = layer + item_offset', 'Wrong layer translation')
    require(manifest['spaces']['buffer']['to_image'] == 'image = buffer + item_offset for drawable-backed buffers', 'Wrong buffer translation')
    require(manifest['spaces']['screen']['origin'] == 'canvas widget top-left', 'Wrong screen origin')
    require(manifest['spaces']['screen']['to_image'] == 'inverse rotation/flip, then add scroll offset, then divide by zoom', 'Wrong inverse order')
    require(manifest['spaces']['image']['origin'] == [0, 0], 'Wrong image origin')
    require(manifest['spaces']['layer']['origin'] == [0, 0], 'Wrong layer origin')
    require(manifest['spaces']['buffer']['origin'] == 'explicit GEGL extent x/y', 'Wrong buffer origin')
    require(manifest['spaces']['screen']['units'] == 'GTK logical pixels', 'Wrong canvas units')
    require(manifest['boundaries']['interval'] == 'half-open', 'Wrong interval')
    require(manifest['boundaries']['read_abyss'] == 'GEGL_ABYSS_NONE when explicitly requested', 'Implicit abyss change')
    for case in manifest['offset_cases']:
        require(len(case['image']) == len(case['offset']) == len(case['buffer']) == 2, 'Malformed point')
        require([p - o for p, o in zip(case['image'], case['offset'])] == case['buffer'], 'Wrong offset direction')
    require(len(manifest['offset_cases']) >= 4, 'Missing offset cases')
    for case in manifest['intersections']:
        require(intersection(case['a'], case['b']) == case['expected'], 'Wrong intersection expectation')
    require(len(manifest['intersections']) >= 10, 'Missing boundary cases')
    processor = sources['app/base/pixel-processor.c']
    for token in ('(* p5_func)', 'PixelRegion         *regions[5]', 'PixelRegion tr[5]',
                  'memcpy (&tr[i], processor->regions[i], sizeof (PixelRegion))', 'processor.regions[4]'):
        require(token in processor, 'Missing processor contract: ' + token)
    require(processor.count('((p5_func) processor->func)') == 2, 'Wrong serial/parallel dispatch coverage')
    region = sources['app/base/pixel-region.c']
    for token in ('PR->data          = temp_buf_get_data (temp_buf)',
                  'PR->rowstride     = temp_buf_get_rowstride (temp_buf)',
                  'PRH->original_data +', 'y * PRH->PR->rowstride +', 'x * PRH->PR->bytes'):
        require(token in region, 'Missing legacy pointer-origin evidence')
    pixel = sources['app/base/pixel.hpp']
    require('typedef guchar  data_t;' in pixel and 'struct Pixel {' in pixel, 'Missing scalar Pixel evidence')
    require(not re.search(r'\b(TempBuf|PixelRegion|GimpCoords|offset_x|offset_y)\b', pixel), 'Pixel gained spatial dependencies')
    drawable = sources['app/paint/gimpmypaintcore-drawablefeature.hpp']
    require('gimp_item_get_offset (drawable_item, &offset_x, &offset_y)' in drawable, 'Missing drawable offset evidence')
    return sources


def legacy_program(sources):
    temp = sources['app/base/temp-buf.c']
    temp_struct = re.search(r'struct _TempBuf\s*\{.*?\};', sources['app/base/temp-buf.h'], re.S).group()
    region_struct = re.search(r'struct _PixelRegion\s*\{.*?\};', sources['app/base/pixel-region.h'], re.S).group()
    iterator_struct = re.search(r'struct _PixelRegionIterator\s*\{.*?\};', sources['app/base/pixel-region.h'], re.S).group()
    proc = sources['app/base/pixel-processor.c']
    typedefs = proc[proc.index('typedef void  (* p1_func)'):proc.index('\n\n#ifdef ENABLE_MP\nstatic void')]
    code = '''#include <glib.h>
#include <stdio.h>
#include <string.h>
typedef struct _TempBuf TempBuf;
typedef struct _PixelRegion PixelRegion;
typedef struct _PixelRegionIterator PixelRegionIterator;
typedef struct _Tile Tile;
typedef struct _TileManager TileManager;
'''
    base_types = sources['app/base/base-types.h']
    code += re.search(r'typedef void\s*\(\*\s*PixelProcessorFunc\).*?;', base_types, re.S).group() + '\n'
    code += re.search(r'typedef void\s*\(\*\s*PixelProcessorProgressFunc\).*?;', sources['app/base/pixel-processor.h'], re.S).group() + '\n'
    code += temp_struct + '\n' + region_struct + '\n' + iterator_struct + '\n' + typedefs + '\n'
    code += '''gint temp_buf_get_rowstride(TempBuf *);
static PixelRegionIterator *pixel_regions_process(PixelRegionIterator *p) {(void)p; return NULL;}
#define PROGRESS_TIMEOUT 128
'''
    for name in ('temp_buf_new', 'temp_buf_get_data', 'temp_buf_get_rowstride', 'temp_buf_subwindow', 'temp_buf_free'):
        code += extract_function(temp, name)
    code += extract_function(sources['app/base/pixel-region.c'], 'pixel_region_init_temp_buf')
    code += extract_function(proc, 'do_parallel_regions_single')
    code += r'''
static int rejected = 0, callbacks = 0;
static void expected_critical(const gchar *domain, GLogLevelFlags level, const gchar *message, gpointer data)
{ (void)domain; (void)level; (void)message; (void)data; rejected++; }
static void release_window(TempBuf *p)
{
  g_assert_nonnull(p->parent);
  temp_buf_free(p); /* Original code leaves the wrapper allocated. */
  g_slice_free(TempBuf, p); /* Harness cleanup, not a legacy fix. */
}
static void dispatch(gpointer data, PixelRegion *a, PixelRegion *b, PixelRegion *c, PixelRegion *d, PixelRegion *e)
{
  PixelRegion *r = data;
  g_assert_true(a == &r[0]); g_assert_null(b); g_assert_true(c == &r[2]);
  g_assert_true(d == &r[3]); g_assert_true(e == &r[4]);
  for (int i = 0; i < 5; i++) if (i != 1) {
    g_assert_cmpint(r[i].x, ==, -7 + 3*i); g_assert_cmpint(r[i].y, ==, 9 - i);
    g_assert_cmpint(r[i].w, ==, 2); g_assert_cmpint(r[i].h, ==, 1);
    g_assert_cmpint(r[i].rowstride, ==, 11 + i);
  }
  callbacks++;
}
int main(void)
{
  guint log_handler = g_log_set_handler(NULL, G_LOG_LEVEL_CRITICAL, expected_critical, NULL);
  for (int bytes = 1; bytes <= 4; bytes++) {
    TempBuf *root = temp_buf_new(7, 5, bytes, -5, 7, NULL);
    for (int i = 0; i < 35*bytes; i++) root->data[i] = (guchar)(i+1);
    g_assert_null(root->parent); g_assert_cmpint(root->x, ==, -5); g_assert_cmpint(root->y, ==, 7);
    TempBuf *inside = temp_buf_subwindow(root, -3, 8, 3, 2);
    g_assert_cmpint(inside->x, ==, -3); g_assert_cmpint(inside->y, ==, 8);
    g_assert_cmpint(inside->width, ==, 3); g_assert_cmpint(inside->height, ==, 2);
    g_assert_true(inside->data == root->data + 9*bytes);
    g_assert_cmpint(temp_buf_get_rowstride(inside), ==, 7*bytes);
    TempBuf *nested = temp_buf_subwindow(inside, -2, 9, 4, 4);
    g_assert_cmpint(nested->x, ==, -2); g_assert_cmpint(nested->y, ==, 9);
    g_assert_cmpint(nested->width, ==, 2); g_assert_cmpint(nested->height, ==, 1);
    g_assert_true(nested->data == root->data + 17*bytes);
    g_assert_cmpint(temp_buf_get_rowstride(nested), ==, 7*bytes);
    PixelRegion region;
    pixel_region_init_temp_buf(&region, inside, 1, 0, 2, 1);
    g_assert_true(region.data == inside->data); g_assert_cmpint(region.x, ==, 1);
    g_assert_cmpint(region.y, ==, 0); g_assert_cmpint(region.rowstride, ==, 7*bytes);
    TempBuf *left = temp_buf_subwindow(root, -9, 5, 6, 4);
    g_assert_cmpint(left->x, ==, -5); g_assert_cmpint(left->y, ==, 7);
    g_assert_cmpint(left->width, ==, 2); g_assert_cmpint(left->height, ==, 2);
    g_assert_true(left->data == root->data);
    TempBuf *right = temp_buf_subwindow(root, 0, 10, 7, 7);
    g_assert_cmpint(right->x, ==, 0); g_assert_cmpint(right->y, ==, 10);
    g_assert_cmpint(right->width, ==, 2); g_assert_cmpint(right->height, ==, 2);
    g_assert_true(right->data == root->data + 26*bytes);
    g_assert_null(temp_buf_subwindow(root, 2, 7, 1, 1));
    g_assert_null(temp_buf_subwindow(root, -5, 12, 1, 1));
    g_assert_null(temp_buf_subwindow(root, -8, 7, 3, 1));
    g_assert_null(temp_buf_subwindow(root, -5, 4, 1, 3));
    g_assert_null(temp_buf_subwindow(root, -5, 7, 0, 1));
    g_assert_null(temp_buf_subwindow(root, -5, 7, 1, 0));
    g_assert_null(temp_buf_subwindow(root, 200, -100, 1, 1));
    release_window(nested); release_window(inside); release_window(left); release_window(right);
    g_assert_cmpint(root->data[0], ==, 1); temp_buf_free(root);
  }
  g_log_remove_handler(NULL, log_handler); g_assert_cmpint(rejected, ==, 28);
  PixelRegion regions[5] = {{0}};
  PixelProcessor processor = {0};
  processor.func = (PixelProcessorFunc)dispatch; processor.data = regions; processor.num_regions = 5;
  for (int i = 0; i < 5; i++) {
    regions[i].x = -7+3*i; regions[i].y = 9-i; regions[i].w = 2; regions[i].h = 1;
    regions[i].rowstride = 11+i; processor.regions[i] = i == 1 ? NULL : &regions[i];
  }
  do_parallel_regions_single(&processor, NULL, NULL, 2);
  g_assert_cmpint(callbacks, ==, 1);
  puts("legacy-window-layouts=4 nested-windows=4 rejected-boundaries=28 independent-region-dispatches=1");
  return 0;
}
'''
    return code


def native_program(manifest, sources):
    code = '''#include <gegl.h>
#include <cairo.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef struct {double scale_x, scale_y; int offset_x, offset_y; cairo_matrix_t *rotate_transform, *rotate_untransform;} GimpDisplayShell;
#define GIMP_IS_DISPLAY_SHELL(s) ((s) != NULL)
#define MIN4(a,b,c,d) MIN(MIN((a),(b)),MIN((c),(d)))
#define MAX4(a,b,c,d) MAX(MAX((a),(b)),MAX((c),(d)))
'''
    math_header = (ROOT / 'libgimpmath/gimpmath.h').read_text()
    require('#define RINT(x) floor ((x) + 0.5)' in math_header, 'Rounding policy changed')
    require('#define RINT(x) floor ((x) + 0.5)' in sources['libgimpmath/gimpmath.h'], 'Legacy rounding evidence missing')
    code += '#define RINT(x) floor ((x) + 0.5)\n'
    shell_header = (ROOT / 'app/display/gimpdisplayshell.h').read_text()
    for name in ('PROJ_ROUND', 'SCALEX', 'SCALEY'):
        macro = re.search(r'^#define\s+' + name + r'\(.*$', shell_header, re.M).group()
        old_macro = re.search(r'^#define\s+' + name + r'\(.*$', sources['app/display/gimpdisplayshell.h'], re.M).group()
        require(macro == old_macro, 'Display rounding macro differs from legacy: ' + name)
        code += macro + '\n'
    native = (ROOT / 'app/display/gimpdisplayshell-transform.c').read_text()
    for name in ('gimp_display_shell_transform_xy_f_noround', 'gimp_display_shell_transform_xy_f',
                 'gimp_display_shell_untransform_xy_f', 'gimp_display_shell_transform_bounds'):
        code += extract_function(native, name)
    legacy = sources['app/display/gimpdisplayshell-transform.c']
    for name in ('gimp_display_shell_zoom_xy_f', 'gimp_display_shell_unzoom_xy_f'):
        code += extract_function(legacy, name).replace(name, 'legacy_' + name)
    code += r'''
static void near(double actual, double expected) {g_assert_cmpfloat(fabs(actual-expected), <, 1e-9);}
int main(int argc, char **argv)
{
  gegl_init(&argc, &argv);
  int rectangle_cases = 0;
'''
    for case in manifest['intersections']:
        a, b = ','.join(map(str, case['a'])), ','.join(map(str, case['b']))
        code += '{ GeglRectangle a={' + a + '}, b={' + b + '}, result;\n'
        code += f'g_assert_cmpint(gegl_rectangle_intersect(&result,&a,&b), ==, {int(case["expected"] is not None)});\n'
        if case['expected']:
            for field, expected in zip(('x', 'y', 'width', 'height'), case['expected']):
                code += f'g_assert_cmpint(result.{field}, ==, {expected});\n'
        code += 'rectangle_cases++; }\n'
    code += r'''
  const Babl *format = babl_format("R'G'B'A u8");
  GeglRectangle extent = {-5,7,3,2}, roi = {-6,6,5,4};
  GeglBuffer *buffer = gegl_buffer_new(&extent, format);
  g_assert_true(gegl_rectangle_equal(gegl_buffer_get_extent(buffer), &extent));
  guchar input[32], output[96]; memset(input, 211, sizeof(input)); memset(output, 199, sizeof(output));
  for (int y=0; y<2; y++) for (int x=0; x<3; x++) for (int c=0; c<4; c++) input[y*16+x*4+c]=1+y*12+x*4+c;
  gegl_buffer_set(buffer, &extent, 0, format, input, 16);
  gegl_buffer_get(buffer, &roi, 1.0, format, output, 24, GEGL_ABYSS_NONE);
  for (int y=0; y<4; y++) {
    for (int x=0; x<5; x++) for (int c=0; c<4; c++) {
      int expected = (x>=1 && x<4 && y>=1 && y<3) ? input[(y-1)*16+(x-1)*4+c] : 0;
      g_assert_cmpint(output[y*24+x*4+c], ==, expected);
    }
    for (int i=20; i<24; i++) g_assert_cmpint(output[y*24+i], ==, 199);
  }
  /* A write crossing the extent changes only the in-extent pixels. */
  GeglRectangle crossing = {-6,7,2,1}; guchar replacement[8]; memset(replacement, 55, sizeof(replacement));
  gegl_buffer_set(buffer, &crossing, 0, format, replacement, GEGL_AUTO_ROWSTRIDE);
  guchar after[24]; gegl_buffer_get(buffer, &extent, 1.0, format, after, GEGL_AUTO_ROWSTRIDE, GEGL_ABYSS_NONE);
  for (int y=0; y<2; y++) for (int x=0; x<3; x++) for (int c=0; c<4; c++)
    g_assert_cmpint(after[y*12+x*4+c], ==, (y==0 && x==0) ? 55 : input[y*16+x*4+c]);
  g_assert_true(gegl_rectangle_equal(gegl_buffer_get_extent(buffer), &extent));
  g_object_unref(buffer);

  GimpDisplayShell shell = {1.5,2.0,7,-9,NULL,NULL};
  double points[][2] = {{-5,7},{0,0},{-0.5,-0.5},{0.25,0.75},{12.25,-3.75},{1,1},{-1,-1},{2.5,-2.5}};
  for (int i=0; i<8; i++) {
    double x,y,oldx,oldy,backx,backy;
    gimp_display_shell_transform_xy_f(&shell,points[i][0],points[i][1],&x,&y);
    legacy_gimp_display_shell_zoom_xy_f(&shell,points[i][0],points[i][1],&oldx,&oldy);
    near(x,oldx); near(y,oldy);
    gimp_display_shell_untransform_xy_f(&shell,x,y,&backx,&backy);
    near(backx, floor(points[i][0]*1.5+0.5)/1.5); near(backy, floor(points[i][1]*2+0.5)/2);
    legacy_gimp_display_shell_unzoom_xy_f(&shell,x,y,&oldx,&oldy);
    near(backx,oldx); near(backy,oldy);
  }
  shell.scale_x=1; shell.scale_y=1; shell.offset_x=0; shell.offset_y=0;
  double x,y; gimp_display_shell_transform_xy_f(&shell,-0.5,-1.5,&x,&y);
  near(x,0); near(y,-1); /* half ties point up, not away from zero */
  for (int mode=0; mode<5; mode++) {
    cairo_matrix_t matrix, inverse;
    cairo_matrix_init_identity(&matrix);
    cairo_matrix_translate(&matrix,23,-13);
    if (mode==1) cairo_matrix_rotate(&matrix,G_PI/2);
    if (mode==2) cairo_matrix_rotate(&matrix,-G_PI/4);
    if (mode==3) cairo_matrix_scale(&matrix,-1,1);
    if (mode==4) cairo_matrix_scale(&matrix,1,-1);
    inverse=matrix; g_assert_cmpint(cairo_matrix_invert(&inverse), ==, CAIRO_STATUS_SUCCESS);
    shell.scale_x=1.5; shell.scale_y=2; shell.offset_x=7; shell.offset_y=-9;
    shell.rotate_transform=&matrix; shell.rotate_untransform=&inverse;
    for (int i=0; i<8; i++) {
      double backx,backy;
      gimp_display_shell_transform_xy_f_noround(&shell,points[i][0],points[i][1],&x,&y);
      gimp_display_shell_untransform_xy_f(&shell,x,y,&backx,&backy);
      near(backx,points[i][0]); near(backy,points[i][1]);
    }
    double x1,y1,x2,y2;
    gimp_display_shell_transform_bounds(&shell,-5,7,2,12,&x1,&y1,&x2,&y2);
    double minx=G_MAXDOUBLE,miny=G_MAXDOUBLE,maxx=-G_MAXDOUBLE,maxy=-G_MAXDOUBLE;
    for (int i=0;i<4;i++) {
      gimp_display_shell_transform_xy_f_noround(&shell,i&1?2:-5,i&2?12:7,&x,&y);
      minx=MIN(minx,x); miny=MIN(miny,y); maxx=MAX(maxx,x); maxy=MAX(maxy,y);
    }
    near(x1,minx); near(y1,miny); near(x2,maxx); near(y2,maxy);
  }
  printf("native-rectangle-cases=%d negative-origin-buffer=1 padded-rows=4 clipped-write=1 rounded-display-pairs=8 affine-roundtrips=40 transformed-bounds=5\n",rectangle_cases);
  gegl_exit(); return 0;
}
'''
    return code


def execute(manifest, sources, sanitize):
    results = []
    with tempfile.TemporaryDirectory(prefix='gimp-coordinate-contract-') as directory:
        directory = Path(directory)
        for name, code, packages in (
            ('legacy', legacy_program(sources), ['glib-2.0']),
            ('native', native_program(manifest, sources), ['gegl-0.4', 'cairo']),
        ):
            source = directory / (name + '.c')
            source.write_text(code)
            binary = directory / name
            command = shlex.split(os.environ.get('CC', 'cc')) + [
                '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-deprecated-declarations', '-g']
            if sanitize:
                command += ['-fsanitize=address,undefined,float-cast-overflow', '-fno-omit-frame-pointer']
            command += [str(source), '-o', str(binary)]
            command += shlex.split(run(['pkg-config', '--cflags', '--libs'] + packages)) + ['-lm']
            run(command)
            environment = os.environ.copy()
            environment.update(ASAN_OPTIONS='detect_leaks=0:halt_on_error=1', UBSAN_OPTIONS='halt_on_error=1',
                               GEGL_USE_OPENCL='no', GEGL_THREADS='1')
            results.append({'program': name, 'output': run([str(binary)], env=environment).strip(),
                            'generated_source_sha256': hashlib.sha256(code.encode()).hexdigest()})
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    sources = validate(manifest)
    results = execute(manifest, sources, args.sanitize)
    paths = [MANIFEST, Path(__file__), ROOT / 'tools/tests/test_coordinate_contract.py',
             ROOT / 'migration/contracts/coordinate-contract.md', ROOT / 'app/display/gimpdisplayshell-transform.c',
             ROOT / 'app/display/gimpdisplayshell.h', ROOT / 'libgimpmath/gimpmath.h']
    report = {'status': 'PASS', 'task': '09.002', 'duties': len(manifest['duties']), 'results': results,
              'sanitizers': ['address', 'undefined', 'float-cast-overflow'] if args.sanitize else [],
              'limits': manifest['non_claims'] + ['Sanitizers cover extracted code/harness; dependency libraries are uninstrumented; LSan disabled'],
              'versions': {name: run(['pkg-config', '--modversion', name]).strip() for name in ('glib-2.0', 'gegl-0.4', 'cairo')},
              'source_sha256': {str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}}
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
