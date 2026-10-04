#!/usr/bin/env python3
"""Check literal Retinex bytes and every allocation failure under sanitizers.

Source tools/linux-debian13-env.sh first, then run with --output REPORT.json.
The sealed Retinex checker verifies and materializes every fixture. This test
compiles exact current source functions with small host/configuration stubs;
it does not replace native PDB, selection, or installed-runtime acceptance.
Analytic invalid geometries use NULL inputs and forbid every allocation.
All generated C, binaries, and raster outputs live in an automatically removed
temporary directory. No shared build directory or historical worktree is used.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile

from check_retinex_evidence import DEFAULT_ARCHIVE, PINNED_MANIFEST, verify

ROOT = Path(__file__).resolve().parents[1]
SOURCE = 'plug-ins/common/contrast-retinex.c'
FUNCTIONS = ('retinex_scales_distribution', 'compute_coefs3', 'gausssmooth',
             'MSRCR', 'compute_mean_var', 'retinex')
SANITIZERS = 'address,undefined,float-cast-overflow'

PREAMBLE = r'''
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdarg.h>
typedef int gint;
typedef float gfloat;
typedef double gdouble;
typedef unsigned char guchar;
typedef int gboolean;
typedef uint64_t guint64;
typedef size_t gsize;
_Static_assert(sizeof(gint) == 4 && sizeof(gfloat) == 4 && sizeof(gdouble) == 8,
               "Expected GIMP integer and floating-point widths");
typedef struct { int scale, nscales, mode; double cvar; } GObject;
typedef struct { int bytes; } Babl;
typedef struct { int width, height, bytes; unsigned char *data; } GimpDrawable;
typedef void GimpPreview;
typedef struct { int references; } GeglBuffer;
#define TRUE 1
#define FALSE 0
#define G_MAXINT INT_MAX
#define G_MAXSIZE SIZE_MAX
#define GIMP_PROCEDURE_CONFIG(x) (x)
#define GIMP_ZOOM_PREVIEW(x) (x)
#define GEGL_RECTANGLE(x,y,w,h) NULL
#define GEGL_AUTO_ROWSTRIDE 0
#define GEGL_ABYSS_NONE 0
#define _(x) (x)
#define CLAMP(x,a,b) (((x)>(b))?(b):(((x)<(a))?(a):(x)))
static int allocations, fail_at, failed_line, live, forbid_allocations;
static int shadow_acquired, writes, merges;
static GeglBuffer src_buffer, dst_buffer;
static GimpDrawable *active;
static void *tracked_alloc(size_t n, int line)
{
  void *p;
  allocations++;
  if (forbid_allocations)
    { fprintf(stderr, "Geometry reached allocation at source line %d\n", line); abort(); }
  if (allocations == fail_at) { failed_line = line; return NULL; }
  p = malloc(n);
  if (!p) { fprintf(stderr, "Unexpected allocator exhaustion\n"); exit(10); }
  live++;
  return p;
}
#define g_try_malloc(n) tracked_alloc((n), __LINE__)
static void g_free(void *p) { if (p) { live--; free(p); } }
#define g_warning(...) ((void)0)
static void g_object_unref(GeglBuffer *p)
{
  if (!p || p->references != 1) abort();
  p->references--;
}
#define g_clear_object(p) do { if (*(p)) g_object_unref(*(p)); *(p)=NULL; } while (0)
static void g_object_get(GObject *c, ...)
{
  va_list args;
  const char *key;
  va_start(args, c);
  while ((key = va_arg(args, const char *)))
    {
      void *value = va_arg(args, void *);
      if (!strcmp(key, "scale")) *(int *)value = c->scale;
      else if (!strcmp(key, "nscales")) *(int *)value = c->nscales;
      else if (!strcmp(key, "cvar")) *(double *)value = c->cvar;
      else abort();
    }
  va_end(args);
}
static int gimp_procedure_config_get_choice_id(GObject *c, const char *name)
{ return c->mode; }
static void gimp_progress_init(const char *text) {}
static void gimp_progress_update(double value) {}
static int gimp_drawable_mask_intersect(GimpDrawable *d, int *x, int *y,
                                         int *width, int *height)
{
  *x = *y = 0; *width = d->width; *height = d->height;
  return *width > 0 && *height > 0;
}
static Babl rgb = {3}, rgba = {4};
static const Babl *gimp_drawable_get_format(GimpDrawable *d)
{ return d->bytes == 3 ? &rgb : &rgba; }
static int gimp_drawable_has_alpha(GimpDrawable *d) { return d->bytes == 4; }
static const Babl *babl_format(const char *format)
{ return strchr(format, 'A') ? &rgba : &rgb; }
static int babl_format_get_bytes_per_pixel(const Babl *format)
{ return format->bytes; }
static guchar *gimp_zoom_preview_get_source(GimpPreview *p, int *w, int *h, int *b)
{ abort(); }
static GeglBuffer *gimp_drawable_get_buffer(GimpDrawable *d)
{ active = d; src_buffer.references++; return &src_buffer; }
static GeglBuffer *gimp_drawable_get_shadow_buffer(GimpDrawable *d)
{ shadow_acquired++; dst_buffer.references++; return &dst_buffer; }
static void gegl_buffer_get(GeglBuffer *buffer, void *rect, double zoom,
                            const Babl *format, guchar *out, int stride, int abyss)
{ memcpy(out, active->data, (size_t)active->width * active->height * active->bytes); }
static void gegl_buffer_set(GeglBuffer *buffer, void *rect, int level,
                            const Babl *format, guchar *in, int stride)
{
  writes++;
  memcpy(active->data, in, (size_t)active->width * active->height * active->bytes);
}
static void gimp_preview_draw_buffer(GimpPreview *p, guchar *s, int stride)
{ abort(); }
static void gimp_drawable_merge_shadow(GimpDrawable *d, int undo) { merges++; }
static void gimp_drawable_update(GimpDrawable *d, int x, int y, int w, int h) {}
static void compute_mean_var(gfloat *, gfloat *, gfloat *, gint, gint);
'''

MAIN = r'''
static int check_geometry(void)
{
  struct { const char *name; int width, height, bytes; } dimensions[] = {
    {"negative-width", -1, 16, 3},
    {"negative-height", 16, -1, 4},
    {"zero-width", 0, 16, 3},
    {"zero-height", 16, 0, 4},
    {"minimum-int-width", INT_MIN, 16, 3},
    {"minimum-int-height", 16, INT_MIN, 4},
    {"signed-product-width", INT_MAX, 16, 3},
    {"signed-product-height", 16, INT_MAX, 4},
    {"signed-product-square", 65536, 65536, 3},
    {"signed-product-rgba", INT_MAX / 4 + 1, 1, 4},
    {"rgb-terminal-index-overflow", INT_MAX / 3, 1, 3},
    {"maximum-int-dimensions", INT_MAX, INT_MAX, 4}
  };
  int channels[] = {0, 1, 2, 5, INT_MAX};
  int recurrence[] = {-1, 0, INT_MAX - 2, INT_MAX - 1, INT_MAX};
  size_t index;
  forbid_allocations = 1;
  printf("{\"dimension_cases\":[");
  for (index = 0; index < sizeof(dimensions) / sizeof(dimensions[0]); index++)
    {
      GimpDrawable drawable = {dimensions[index].width, dimensions[index].height,
                               dimensions[index].bytes, NULL};
      if (retinex(NULL, &drawable, NULL, TRUE) ||
          MSRCR(NULL, NULL, drawable.width, drawable.height, drawable.bytes, FALSE, TRUE))
        return 11;
      printf("%s\"%s\"", index ? "," : "", dimensions[index].name);
    }
  printf("],\"invalid_channel_counts\":[");
  for (index = 0; index < sizeof(channels) / sizeof(channels[0]); index++)
    {
      if (MSRCR(NULL, NULL, 16, 16, channels[index], FALSE, TRUE)) return 12;
      printf("%s%d", index ? "," : "", channels[index]);
    }
  printf("],\"recurrence_sizes\":[");
  for (index = 0; index < sizeof(recurrence) / sizeof(recurrence[0]); index++)
    {
      if (gausssmooth(NULL, NULL, recurrence[index], 1, NULL)) return 13;
      printf("%s%d", index ? "," : "", recurrence[index]);
    }
  if (allocations || live || shadow_acquired || writes || merges ||
      src_buffer.references || dst_buffer.references) return 14;
  printf("],\"retinex_dimension_rejections\":%zu,\"kernel_dimension_rejections\":%zu,"
         "\"kernel_channel_rejections\":%zu,\"recurrence_rejections\":%zu,"
         "\"allocation_calls\":0,\"shadow_operations\":0}\n",
         sizeof(dimensions) / sizeof(dimensions[0]),
         sizeof(dimensions) / sizeof(dimensions[0]),
         sizeof(channels) / sizeof(channels[0]),
         sizeof(recurrence) / sizeof(recurrence[0]));
  return 0;
}

int main(int argc, char **argv)
{
  GimpDrawable drawable;
  GObject config;
  size_t size;
  unsigned char *original;
  FILE *stream;
  int ok, legacy;
  if (argc == 2 && !strcmp(argv[1], "--geometry")) return check_geometry();
  if (argc != 12) return 2;
  drawable = (GimpDrawable){atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), NULL};
  config = (GObject){atoi(argv[6]), atoi(argv[7]), atoi(argv[8]), strtod(argv[9], NULL)};
  legacy = atoi(argv[10]); fail_at = atoi(argv[11]);
  size = (size_t)drawable.width * drawable.height * drawable.bytes;
  drawable.data = malloc(size); original = malloc(size);
  if (!drawable.data || !original) return 3;
  stream = fopen(argv[1], "rb");
  if (!stream || fread(drawable.data, 1, size, stream) != size) return 4;
  fclose(stream);
  memcpy(original, drawable.data, size);
  ok = retinex(&config, &drawable, NULL, legacy);
  if (live || src_buffer.references || dst_buffer.references) return 5;
  if (fail_at)
    {
      if (!failed_line || ok || shadow_acquired || writes || merges ||
          memcmp(original, drawable.data, size)) return 6;
    }
  else if (!ok || failed_line || shadow_acquired != 1 || writes != 1 || merges != 1)
    return 7;
  stream = fopen(argv[2], "wb");
  if (!stream || fwrite(drawable.data, 1, size, stream) != size) return 8;
  fclose(stream);
  printf("{\"success\":%d,\"allocations\":%d,\"failed_line\":%d,"
         "\"shadow_acquired\":%d,\"writes\":%d,\"merges\":%d,"
         "\"live_allocations\":%d,\"buffer_references\":%d}\n",
         ok, allocations, failed_line, shadow_acquired, writes, merges, live,
         src_buffer.references + dst_buffer.references);
  free(original); free(drawable.data);
  return 0;
}
'''


def digest(data):
    return hashlib.sha256(data).hexdigest()


def extracted_source(source):
    """Copy complete functions verbatim and retain their original line numbers."""
    pieces, functions = [], {}
    for name in FUNCTIONS:
        matches = list(re.finditer(r'static (?:void|gboolean)\n' + name + r' \(', source))
        if len(matches) != 1:
            raise ValueError('Expected one source definition: ' + name)
        start = matches[0].start()
        end = source.index('\n}\n', start) + 3
        text = source[start:end]
        line = source.count('\n', 0, start) + 1
        pieces.append(f'#line {line} "{SOURCE}"\n' + text)
        functions[name] = dict(line=line, sha256=digest(text.encode()))
    constants = []
    for name in ('MAX_RETINEX_SCALES', 'RETINEX_UNIFORM', 'RETINEX_LOW', 'RETINEX_HIGH'):
        constants.append(re.search(r'^#define ' + name + r'\s+\d+\s*$', source, re.M)[0])
    constants.append(re.search(r'^static gfloat RetinexScales\[.*?\];$', source, re.M)[0])
    constants.append(re.search(r'typedef struct\n\{[^}]+\} gauss3_coefs;', source)[0])
    exact = '\n'.join(constants + pieces)
    sites = {}
    for match in re.finditer(r'^\s*(\w+)\s*=\s*(?:\(gfloat \*\)\s*)?g_try_malloc \(', source, re.M):
        name = match[1]
        line = source.count('\n', 0, match.start(1)) + 1
        if name in sites:
            raise ValueError('Duplicate allocation site: ' + name)
        sites[name] = dict(line=line, injected_failures_passed=0)
    if set(sites) != {'src', 'dst', 'in', 'out', 'w1', 'w2'}:
        raise ValueError('Retinex allocation sites changed; update the proof')
    return exact, functions, sites


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--archive', type=Path, default=DEFAULT_ARCHIVE)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit('Existing evidence is sealed; choose a new output report')
    inputs = [SOURCE, 'tools/check_retinex_allocations.py',
              'tools/check_retinex_evidence.py', 'tools/derive_retinex_evidence.py']
    before = {name: digest((ROOT/name).read_bytes()) for name in inputs}
    exact, functions, sites = extracted_source((ROOT/SOURCE).read_text())
    generated = PREAMBLE + '\n' + exact + '\n#line 1 "retinex-allocation-main.c"\n' + MAIN
    compiler = shlex.split(args.cc)
    if not compiler:
        raise ValueError('Empty compiler command')
    flags = ['-std=c11', '-O2', '-Wall', '-Wno-unused-function',
             '-fsanitize=' + SANITIZERS, '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:abort_on_error=1',
           'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
    report = dict(schema_version=1, status='failed', source_sha256_before=before,
                  extracted_functions=functions, extracted_source_sha256=digest(exact.encode()),
                  generated_harness_sha256=digest(generated.encode()),
                  sanitizers=SANITIZERS.split(','), compiler=compiler, compiler_flags=flags,
                  sanitizer_options={k: env[k] for k in ('ASAN_OPTIONS', 'UBSAN_OPTIONS')},
                  leak_check='All six scratch allocation sites and both buffer references are counted; '
                             'LeakSanitizer is disabled because this executor uses ptrace.',
                  scope='Exact source kernel and retinex publication control with stubbed host/configuration; '
                        'native PDB, selection compositing, and installed behavior require separate tests.',
                  allocation_sites=sites, allocation_scenarios=[],
                  hidden_kernel_cases_passed=0, legacy_visible_and_alpha_cases_passed=0,
                  public_kernel_cases_passed=0, allocation_failures_passed=0)
    try:
        report['compiler_version'] = subprocess.check_output(compiler + ['--version'], text=True).splitlines()[0]
        with tempfile.TemporaryDirectory(prefix='retinex-allocations-') as temporary:
            scratch = Path(temporary)
            manifest = verify(args.archive, scratch/'evidence')
            report['archive_sha256'] = manifest['archive_sha256']
            report['manifest_sha256'] = PINNED_MANIFEST
            base = scratch/'evidence/retinex-evidence'
            harness, binary, output = scratch/'harness.c', scratch/'harness', scratch/'actual.raw'
            harness.write_text(generated)
            command = compiler + flags + [str(harness), '-lm', '-o', str(binary)]
            built = subprocess.run(command, capture_output=True, text=True, timeout=60)
            if built.returncode:
                raise RuntimeError('Harness compilation failed:\n' + built.stdout + built.stderr)
            report['compiler_diagnostics'] = (built.stdout + built.stderr).strip()
            report['harness_binary_sha256'] = digest(binary.read_bytes())
            geometry = subprocess.run([str(binary), '--geometry'], capture_output=True,
                                      text=True, env=env, timeout=15)
            if geometry.returncode or geometry.stderr:
                raise RuntimeError('Analytic geometry rejection failed:\n' +
                                   geometry.stdout + geometry.stderr)
            report['analytic_geometry'] = json.loads(geometry.stdout)
            report['analytic_geometry']['null_pixel_and_config_inputs'] = True
            report['analytic_geometry']['allocator_aborts_if_reached'] = True
            report['analytic_geometry_rejections_passed'] = sum(
                report['analytic_geometry'][key] for key in ('retinex_dimension_rejections',
                'kernel_dimension_rejections', 'kernel_channel_rejections', 'recurrence_rejections'))

            def run(case, legacy, failure=0):
                group = 'hidden-integrated' if legacy else 'public-before'
                original = base/group/case['input']
                command = [str(binary), str(original), str(output), str(case['width']),
                           str(case['height']), str(case['channels']),
                           *map(str, case['parameters']), str(int(legacy)), str(failure)]
                result = subprocess.run(command, capture_output=True, text=True, env=env, timeout=15)
                if result.returncode or result.stderr:
                    raise RuntimeError(f"Harness failed {case['id']} allocation={failure} "
                                       f"exit={result.returncode}:\n{result.stdout}{result.stderr}")
                stats = json.loads(result.stdout)
                if stats['live_allocations'] or stats['buffer_references']:
                    raise AssertionError('Unbalanced allocation or buffer reference')
                actual = output.read_bytes()
                if failure:
                    if (stats['success'] or not stats['failed_line'] or
                        any(stats[key] for key in ('shadow_acquired', 'writes', 'merges')) or
                        actual != original.read_bytes()):
                        raise AssertionError('Allocation failure published pixels or reported success')
                return actual, stats

            hidden = json.loads((base/'hidden-integrated/index.json').read_text())['cases']
            public = json.loads((base/'public-before/index.json').read_text())['cases']
            for cases, legacy, key in ((hidden, True, 'hidden_kernel_cases_passed'),
                                       (public, False, 'public_kernel_cases_passed')):
                group = 'hidden-integrated' if legacy else 'public-before'
                for case in cases:
                    actual, _ = run(case, legacy)
                    if actual != (base/group/case['output']).read_bytes():
                        raise AssertionError('Captured kernel bytes changed: ' + case['id'])
                    if legacy:
                        visible = bytearray(actual)
                        original = (base/group/case['input']).read_bytes()
                        if case['channels'] == 4:
                            for at in range(0, len(original), 4):
                                if original[at+3] == 0:
                                    visible[at:at+3] = original[at:at+3]
                        if visible != (base/'pdb'/case['output']).read_bytes():
                            raise AssertionError('Old visible/alpha bytes changed: ' + case['id'])
                        report['legacy_visible_and_alpha_cases_passed'] += 1
                    report[key] += 1
            by_line = {site['line']: name for name, site in sites.items()}
            for channels in (3, 4):
                case = next(case for case in hidden if case['channels'] == channels and
                            (case['width'], case['height']) == (16, 16) and case['parameters'][1] == 3)
                _, normal = run(case, True)
                counts = dict.fromkeys(sites, 0)
                for failure in range(1, normal['allocations'] + 1):
                    _, stats = run(case, True, failure)
                    site = by_line[stats['failed_line']]
                    counts[site] += 1
                    sites[site]['injected_failures_passed'] += 1
                    report['allocation_failures_passed'] += 1
                if set(counts.values()) == {0} or any(not count for count in counts.values()):
                    raise AssertionError('An allocation site was not exercised')
                report['allocation_scenarios'].append(dict(case_id=case['id'],
                    input_sha256=digest((base/'hidden-integrated'/case['input']).read_bytes()),
                    width=case['width'], height=case['height'], channels=channels,
                    parameters=case['parameters'], allocation_calls=normal['allocations'],
                    injected_failures_by_site=counts))
        report['source_sha256_after'] = {name: digest((ROOT/name).read_bytes()) for name in inputs}
        if before != report['source_sha256_after']:
            raise AssertionError('Source changed during verification')
        report['assertions'] = dict(all_allocations_injected=True, source_pixels_unchanged_on_failure=True,
            no_shadow_acquisition_on_failure=True, no_shadow_write_on_failure=True,
            no_shadow_merge_on_failure=True, no_tracked_allocation_leaks=True,
            no_buffer_reference_leaks=True, source_files_unchanged=True, no_sanitizer_diagnostics=True)
        report['status'] = 'passed'
    except Exception as error:
        report['error'] = str(error)
        raise
    finally:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({key: report[key] for key in ('status', 'hidden_kernel_cases_passed',
                     'public_kernel_cases_passed', 'allocation_failures_passed',
                     'analytic_geometry_rejections_passed')}))


if __name__ == '__main__':
    main()
