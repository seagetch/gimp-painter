#!/usr/bin/env python3
"""Audit pinned source witnesses and execute isolated behavioral counterexamples.

This does not run GIMP or certify full tool/file compatibility.
"""
import argparse
import csv
import io
import json
import re
import subprocess
import tempfile
from pathlib import Path

from inventory_enum_deltas import parse

ROOT = Path('migration/inventory')


def source(commit, path):
    return subprocess.check_output(['git', 'show', f'{commit}:{path}'], text=True)


def line(text, anchor):
    if not anchor or anchor not in text:
        raise ValueError(f'missing source anchor: {anchor!r}')
    return text[:text.index(anchor)].count('\n') + 1


def counterexamples(old, target):
    legacy_rotation = source(old, 'app/display/gimpdisplayshell-tool-events.c')
    current_rotation = source(target, 'app/display/gimpdisplayshell-rotate.c')
    # Compile the actual rounding expressions, not a Python approximation.
    legacy_expr = re.search(r'angle = (\(gint\).*ROTATE_SNAP_UNIT);', legacy_rotation)[1]
    current_expr = re.search(r'(RINT \(shell->rotate_drag_angle / 15\.0\) \* 15\.0)', current_rotation)[1]
    legacy_motion = source(old, 'app/display/gimpmotionbuffer.c')
    current_motion = source(target, 'app/display/gimpmotionbuffer.c')
    pattern = r'if \((fabs \(delta_x\) < filter\s*&&\s*fabs \(delta_y\) < filter(?:\s*&&\s*delta_pressure == 0\.0)?)\)'
    old_condition = re.search(pattern, legacy_motion)[1]
    new_condition = re.search(pattern, current_motion)[1]
    math_header = source(target, 'libgimpmath/gimpmath.h')
    assert '#define RINT(x) rint(x)' in math_header
    assert '#define RINT(x) floor ((x) + 0.5)' in math_header
    code = '''#include <assert.h>
#include <math.h>
#include <fenv.h>
#include <stdio.h>
typedef int gint;
#define ROTATE_SNAP_UNIT 15.0
#ifdef WITNESS_FLOOR_RINT
#define RINT(x) floor ((x) + 0.5)
#else
#define RINT(x) rint(x)
#endif
typedef struct { double rotate_drag_angle; } Shell;
static double old_snap(double angle) { return OLD_EXPR; }
static double new_snap(double angle) { Shell s={angle}; Shell *shell=&s; return NEW_EXPR; }
static int old_drop(double delta_x,double delta_y,double filter,double delta_pressure) { return OLD_DROP; }
static int new_drop(double delta_x,double delta_y,double filter,double delta_pressure) { (void)delta_pressure; return NEW_DROP; }
int main(void) {
  assert(fesetround(FE_TONEAREST)==0);
#ifdef WITNESS_FLOOR_RINT
  assert(old_snap(7.5)==15.0 && new_snap(7.5)==15.0);
  assert(old_snap(37.5)==45.0 && new_snap(37.5)==45.0);
#else
  assert(old_snap(7.5)==15.0 && new_snap(7.5)==0.0);
  assert(old_snap(37.5)==45.0 && new_snap(37.5)==30.0);
#endif
  assert(old_snap(7.499)==0.0 && new_snap(7.499)==0.0);
  assert(old_snap(7.501)==15.0 && new_snap(7.501)==15.0);
  assert(old_drop(0,0,.1,-.6)==0 && new_drop(0,0,.1,-.6)==1);
  assert(old_drop(0,0,.1,0)==1 && new_drop(0,0,.1,0)==1);
  puts("rotation tie and stationary-pressure counterexamples: PASS");
}
'''
    for marker, expression in [('OLD_EXPR', legacy_expr), ('NEW_EXPR', current_expr),
                               ('OLD_DROP', old_condition), ('NEW_DROP', new_condition)]:
        code = code.replace(marker, expression)
    with tempfile.TemporaryDirectory() as temp:
        c = Path(temp) / 'witness.c'
        executable = Path(temp) / 'witness'
        c.write_text(code)
        for defines in ([], ['-DWITNESS_FLOOR_RINT']):
            subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', *defines,
                            str(c), '-lm', '-o', str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
    a = parse(source(old, 'app/xcf/xcf-private.h'))
    b = parse(source(target, 'app/xcf/xcf-private.h'))
    assert a['PROP_FILTER_SPEC'][0] == b['PROP_LOCK_POSITION'][0] == '32'
    assert a['PROP_CLONE_SPEC'][0] == b['PROP_FLOAT_OPACITY'][0] == '33'
    print('XCF property 32/33 conflicts: PASS')


def render():
    baseline = json.loads(Path('migration/baseline/baseline.json').read_text())
    old = baseline['source']['commit']
    targets = [('published', baseline['published_comparison']['commit']),
               ('initial-port', baseline['initial_port']['upstream_commit'])]
    manifest = json.loads((ROOT / 'upstream-contracts.json').read_text())
    wbs = Path('tasks.md').read_text().split('## 4. WBS\n')[1].split('## 5. ')[0]
    ids = set(re.findall(r'^\|\s*(\d{2}\.\d{3}(?:/[\w.-]+)?)\s*\|', wbs, re.M))
    if len({r['key'] for r in manifest}) != len(manifest):
        raise ValueError('duplicate contract keys')
    out = io.StringIO()
    writer = csv.writer(out, delimiter='\t', lineterminator='\n')
    writer.writerow(('child_id', 'status', 'features', 'target_commit', 'legacy_site',
                     'target_site', 'relation', 'contract', 'followup', 'behavior_evidence'))
    for role, commit in targets:
        paths = set(subprocess.check_output(['git', 'ls-tree', '-r', '--name-only', commit], text=True).splitlines())
        for row in manifest:
            for task in row['followup'].split(','):
                if task not in ids:
                    raise ValueError(f'{row["key"]}: unknown followup {task}')
            old_text = source(old, row['legacy_path'])
            old_site = f'{row["legacy_path"]}:{line(old_text, row["legacy_anchor"])}'
            p = row['target_path']
            if row['relation'] == 'TARGET_PATH_ABSENT':
                if p in paths:
                    raise ValueError(f'{p} unexpectedly exists')
                new_site = p + ':ABSENT (same-path check only)'
            else:
                new_text = source(commit, p)
                new_site = f'{p}:{line(new_text, row["target_anchor"])}'
                if row['relation'] == 'IDENTICAL_HEADER_ONLY' and old_text != new_text:
                    raise ValueError('header no longer identical')
            writer.writerow((f'01.012/{row["key"]}-{role}', 'DONE', row['features'], commit,
                             old_site, new_site, row['relation'], row['contract'],
                             row['followup'], row['behavior_evidence']))
        counterexamples(old, commit)
    return out.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    result = render()
    path = ROOT / 'upstream-contract-review.tsv'
    if args.check:
        if path.read_text() != result:
            raise SystemExit('contract review changed; regenerate and review')
    else:
        path.write_text(result)
    print(f'{len(result.splitlines()) - 1} pinned contract reviews: PASS')


if __name__ == '__main__':
    main()
