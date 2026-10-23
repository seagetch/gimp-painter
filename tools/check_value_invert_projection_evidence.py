#!/usr/bin/env python3
"""Verify genuine Value Invert projection-copy/cache-history observations.

Directory validation accepts any nonempty subset of the finite matrix. Sealed
verification requires all 16 scenes and distinguishing cache/hidden-RGB evidence.
Archive pins remain empty until an independently inspected genuine capture.
"""
from __future__ import annotations
import argparse
import gzip
import hashlib
import json
from pathlib import Path
from check_value_invert_evidence import (read_archive, generated_source, raw_pixel,
                                    check_merge, check_signature,
                                    check_runtime_diagnostics)

ROOT = Path(__file__).resolve().parents[1]
COMMIT = 'afa43fae3e920210146abed514f136fd49f671b5'
DEFAULT = ROOT/'migration/fixtures/legacy-value-invert-projection.tar.xz'
ARCHIVE_SHA256 = '25e8e96d012a2d291d6bf42413072e9f9eef80a9ba202e731cd6e21ddbb15b04'
MANIFEST_SHA256 = 'c1783b225c7bb026e36ed49dcf1e64707d9f863d20bdec20534bf67de153371b'
REPORT_SHA256 = '053a0c99fe7f1a5360dbf4565f297cfea44ff19b05b4254f22e7ea338ddd75e2'
COLUMNS = ('id source_channels grouped seeded initial_x initial_y final_x final_y '
           'starts ends merges copies initial_loaded_starts initial_loaded_ends').split()
COPY_COLUMNS = ('id run channels width height filter_x filter_y parent_x parent_y parent_width parent_height '
                'source_x source_y dest_x dest_y copy_width copy_height starts ends merges').split()
OPTION_COLUMNS = ['id', 'run', 'run_mode']
COMPLETION = b'LEGACY_VALUE_INVERT_PROJECTION_CAPTURE_COMPLETE'


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def table(files, name, columns):
    rows = [line.split('\t') for line in files[name].decode().splitlines()]
    if any(len(row) != columns for row in rows):
        raise ValueError('Invalid projection table '+name)
    return rows


def check_plan(tests, full=False):
    """Check bounded scene descriptions without requiring a complete probe run."""
    expected = {(c, g, n, s) for c in (3, 4) for g in (0, 1)
                for n in (0, 1) for s in (0, 1)}
    actual = set()
    fixed = dict(width=17, height=13, image_width=13, image_height=11,
                 live=1, selection=0, final_selection=-1,
                 active_components=15, final_components=-1, lock_alpha=0,
                 final_lock=-1, zero_alpha=0, rerun=0, writer=0, phase=0,
                 idle_selection=-1, idle_components=-1, idle_lock=-1)
    for case in tests:
        c, grouped, seeded = case['components'], case['grouped'], case['seeded']
        negative = int(case['offset_x'] < 0)
        key = (c, grouped, negative, seeded)
        ident = (f"projection-c{c}-{'group' if grouped else 'top'}-"
                 f"{'negative' if negative else 'positive'}-{'seeded' if seeded else 'fresh'}")
        offset = (-4, -3) if negative else (3, 2)
        if (key not in expected or key in actual or case['id'] != ident or
            (case['offset_x'], case['offset_y']) != offset or
            any(case.get(name) != value for name, value in fixed.items())):
            raise ValueError('Invalid or repeated Value Invert projection scene '+case['id'])
        actual.add(key)
    if not actual or (full and actual != expected):
        raise ValueError('Incomplete Value Invert projection coverage matrix')


def check_events(events, ident):
    """Require actual observer ordering/counts, allowing only in-run progress."""
    local = [row for row in events if row[0] == ident]
    phases = []
    starts = ends = merges = 0
    for row in local:
        event = row[1]
        observed = (int(row[2]), int(row[3]), int(row[5]))
        if event == 'progress':
            if (starts != ends+1 or observed != (starts, ends, merges) or
                not 0 <= float(row[4]) <= 1):
                raise ValueError('Progress outside actual run '+ident)
            continue
        phases.append(event)
        if event == 'projection-copy':
            expected = (starts, ends, merges)
        elif event == 'start':
            starts += 1
            expected = (starts, ends, merges)
        elif event == 'merge-before':
            merges += 1
            expected = (starts, ends, merges)
        elif event == 'merge-after':
            expected = (starts, ends, merges)
        elif event == 'end':
            ends += 1
            expected = (starts, ends, merges)
        else:
            raise ValueError('Unexpected projection phase '+ident+' '+event)
        if observed != expected or float(row[4]) != 0:
            raise ValueError('Wrong actual projection phase counts '+ident)
    if phases != ['projection-copy', 'start', 'merge-before', 'merge-after', 'end']*3:
        raise ValueError('Missing, repeated or reordered actual projection phase '+ident)


def observations(files, tests):
    check_plan(tests)
    check_signature(files)
    rows = table(files, 'captures.tsv', len(COLUMNS))
    copies = table(files, 'copies.tsv', len(COPY_COLUMNS))
    options = table(files, 'options.tsv', len(OPTION_COLUMNS))
    events = table(files, 'events.tsv', 6)
    rois = table(files, 'rois.tsv', 7)
    merges = table(files, 'merges.tsv', 7)
    bounds = table(files, 'bounds.tsv', 15)
    if table(files, 'statuses.tsv', 2):
        raise ValueError('Unexpected direct-PDB statuses in live projection capture')
    ids = {case['id'] for case in tests}
    runs = {(ident, run) for ident in ids for run in (1, 2, 3)}
    merge_ids = {ident+f'-merge{run}' for ident, run in runs}
    row_map = {row[0]: row for row in rows}
    copy_map = {(row[0], int(row[1])): row for row in copies}
    option_map = {(row[0], int(row[1])): int(row[2]) for row in options}
    roi_map = {(row[0], int(row[1])): list(map(int, row[2:])) for row in rois}
    merge_map = {row[0]: list(map(int, row[1:])) for row in merges}
    bounds_map = {row[0]: list(map(int, row[1:])) for row in bounds}
    for observed, source, expected in ((row_map, rows, ids), (copy_map, copies, runs),
            (option_map, options, runs), (roi_map, rois, runs),
            (merge_map, merges, merge_ids), (bounds_map, bounds, merge_ids)):
        if len(observed) != len(source) or set(observed) != expected:
            raise ValueError('Incomplete or duplicate projection observations')
    if {row[0] for row in events} != ids:
        raise ValueError('Unexpected or missing projection event scene')
    records = []
    proof = dict(preserved_after_first_run_pixels=0, nonzero_preserved_pixels=0,
                 retained_pixels_changed_from_seed=0, group_outside_image_pixels=0,
                 raw_pixels_checked=0, zero_alpha_hidden_color_channels=0)
    for case in tests:
        ident = case['id']; c = 4
        w = case['width']; h = case['height']; size = w*h*c
        source_size = w*h*case['components']
        final_x, final_y = (-4, -3) if case['offset_x'] >= 0 else (3, 2)
        expected = [case['components'], case['grouped'], case['seeded'],
                    case['offset_x'], case['offset_y'], final_x, final_y, 3, 3, 3, 3,
                    0 if case['seeded'] else -1, 0 if case['seeded'] else -1]
        if list(map(int, row_map[ident][1:])) != expected:
            raise ValueError('Wrong projection counts/context '+ident)
        check_events(events, ident)
        buffers = {}

        def take(label, length=size):
            name = ident+'-'+label+'.raw'; raw = files[name]
            if len(raw) != length:
                raise ValueError('Wrong projection buffer extent '+name)
            buffers[label] = dict(file=name, bytes=length, sha256=sha(raw))
            return raw

        seed = take('seed'); source = take('source', source_size); take('input')
        expected_seed = generated_source(dict(case, components=4), 2) if case['seeded'] else bytes(size)
        if source != generated_source(case, 0) or seed != expected_seed:
            raise ValueError('Non-synthetic seed/source '+ident)
        mask = take('start-mask', case['image_width']*case['image_height'])
        if any(mask):
            raise ValueError('Projection supplement selection is not empty '+ident)
        if case['seeded'] and take('loaded-cache') != seed:
            raise ValueError('Loaded initial cache changed '+ident)
        previous = seed; run_records = []
        for run in (1, 2, 3):
            if option_map[ident, run] != 1:
                raise ValueError('Wrong observed Value Invert retained noninteractive context '+ident)
            row = copy_map[ident, run]
            q = dict(zip(COPY_COLUMNS, [row[0]]+list(map(int, row[1:]))))
            x, y = (case['offset_x'], case['offset_y']) if run < 3 else (final_x, final_y)
            if [q[k] for k in ('channels', 'width', 'height', 'filter_x', 'filter_y', 'starts', 'ends', 'merges')] != [c, w, h, x, y, run-1, run-1, run-1]:
                raise ValueError('Wrong actual copy phase or RGBA drawable geometry '+ident)
            if case['grouped']:
                px = min(x, case['offset_x']); py = min(y, case['offset_y'])
                pw = max(x, case['offset_x'])+w-px; ph = max(y, case['offset_y'])+h-py
            else:
                px = py = 0; pw = case['image_width']; ph = case['image_height']
            if [q[k] for k in ('parent_x', 'parent_y', 'parent_width', 'parent_height')] != [px, py, pw, ph]:
                raise ValueError('Observed ordinary parent projection bounds differ '+ident)
            dx = max(0, px-x); dy = max(0, py-y); sx = max(0, x-px); sy = max(0, y-py)
            cw = min(w-dx, pw-sx); ch = min(h-dy, ph-sy)
            if [q[k] for k in ('source_x', 'source_y', 'dest_x', 'dest_y', 'copy_width', 'copy_height')] != [sx, sy, dx, dy, cw, ch]:
                raise ValueError('Observed copy does not match parent intersection '+ident)
            before = take(f'before{run}'); projection = take(f'projection{run}', pw*ph*c)
            start = take(f'start{run}')
            start_mask = take(f'start{run}-mask', case['image_width']*case['image_height'])
            if any(start_mask) or before != previous:
                raise ValueError('Prior cache was not retained before copy '+ident)
            copied = bytearray(before)
            for yy in range(ch):
                dest = ((dy+yy)*w+dx)*c; src = ((sy+yy)*pw+sx)*c
                copied[dest:dest+cw*c] = projection[src:src+cw*c]
            if start != copied:
                raise ValueError('Actual parent copy did not preserve untouched cache '+ident)
            if run == 1 and start != files[ident+'-input.raw']:
                raise ValueError('First input/start disagreement '+ident)
            for yy in range(h):
                for xx in range(w):
                    at = (yy*w+xx)*c
                    if run == 2 and not (dx <= xx < dx+cw and dy <= yy < dy+ch):
                        proof['preserved_after_first_run_pixels'] += 1
                        proof['nonzero_preserved_pixels'] += int(any(start[at:at+c]))
                        proof['retained_pixels_changed_from_seed'] += int(start[at:at+c] != seed[at:at+c])
                    if case['grouped'] and not (0 <= x+xx < case['image_width'] and 0 <= y+yy < case['image_height']):
                        proof['group_outside_image_pixels'] += 1
            if take(f'source{run}', source_size) != source:
                raise ValueError('Unchanged lower notification changed source '+ident)
            if roi_map[ident, run] != [0, 0, 0, w, h]:
                raise ValueError('Unexpected start selection bounds '+ident)
            merge_id = ident+f'-merge{run}'
            if merge_map[merge_id] != [w, h, c, 15, 1, 0]:
                raise ValueError('Unexpected final merge context '+ident)
            # All selection masks are empty. The old channel reports the whole
            # image as its empty bounds, and item mask bounds cover the drawable.
            if bounds_map[merge_id] != [w, h, x, y, 1, 0, 0, case['image_width'], case['image_height'], 0, 0, 0, w, h]:
                raise ValueError('Unexpected actual merge bounds '+ident)
            merge_input = take(f'merge{run}-input'); shadow = take(f'merge{run}-shadow')
            coverage = take(f'merge{run}-mask', w*h)
            output = take(f'output{run}'); merged = take(f'merge{run}-output')
            if merge_input != start or coverage != bytes([255])*(w*h):
                raise ValueError('Unexpected actual merge input/mask '+ident)
            for at in range(0, size, c):
                if shadow[at:at+c] != bytes(raw_pixel(start[at:at+c])):
                    raise ValueError('Actual raw Value Invert shadow differs '+ident+f' run {run} pixel {at//c}')
                proof['raw_pixels_checked'] += 1
            proof['zero_alpha_hidden_color_channels'] += check_merge(start, shadow, coverage, output, c, 15)
            if merged != output:
                raise ValueError('Final drawable differs from actual REPLACE_INTEN merge '+ident)
            run_records.append(dict(run=run, run_mode=1, copy=q)); previous = output
        records.append(dict(case, observed=dict(zip(COLUMNS, [row_map[ident][0]]+list(map(int, row_map[ident][1:])))),
                            runs=run_records, buffers=buffers))
    return records, proof


def validate_directory(out, tests, code):
    out = Path(out)
    files = {path.name: path.read_bytes() for path in out.iterdir() if path.is_file()}
    errors = []; records = []; proof = {}
    if code:
        errors.append('Harness exit '+str(code))
    try:
        check_runtime_diagnostics(files.get('capture.log', b''))
    except ValueError as failure:
        errors.append(str(failure))
    if COMPLETION not in files.get('capture.log', b''):
        errors.append('No successful Value Invert projection capture completion')
    try:
        records, proof = observations(files, tests)
    except (KeyError, ValueError, TypeError, IndexError) as failure:
        errors.append(str(failure))
    (out/'fixtures.json').write_text(json.dumps(dict(
        schema_version=1, capture_columns=COLUMNS, copy_columns=COPY_COLUMNS,
        option_columns=OPTION_COLUMNS, cases=records, proof=proof,
        loaded_policy='Synthetic seeded pixels use the actual mark_as_loaded API with zero initial jobs; no actual XCF reopening is claimed.',
        group_policy='Ordinary automatic group bounds, measured by pre-copy observer; no forced clipping/resize.',
        source_policy='All source/sourceN buffers are unchanged deterministic synthetic RGB/RGBA lower pixels; owned filter/projection buffers are always RGBA.'), indent=2)+'\n')
    return errors, len(records)


def check_capture(files, report_hash):
    if not report_hash or sha(files['capture-report.json']) != report_hash:
        raise ValueError('Changed or unsealed Value Invert projection report')
    report = json.loads(files['capture-report.json'])
    if (report['status'] != 'passed' or report['errors'] or report['exit_code'] != 0 or
        report['source_commit'] != COMMIT or report['large_cases'] or
        report['case_count'] != 16 or report['expected_cases'] != 16 or
        report['source_sha256'] != report['post_capture_source_sha256'] or
        report['executable_sha256'] != report['post_capture_executable_sha256'] or
        'plug-ins/common/value-invert.c' not in report['source_sha256']):
        raise ValueError('Invalid Value Invert projection provenance')
    from capture_legacy_value_invert_projection import SOURCES, VALUE_INVERT_SOURCE_SHA256
    if set(report['source_sha256']) != set(SOURCES) or report['source_sha256'].get('plug-ins/common/value-invert.c') != VALUE_INVERT_SOURCE_SHA256:
        raise ValueError('Wrong Value Invert projection source pins')
    if set(files) != set(report['files_sha256']) | {'capture-report.json'}:
        raise ValueError('Unexpected projection artifacts')
    for name, digest in report['files_sha256'].items():
        if sha(files[name]) != digest:
            raise ValueError('Changed projection artifact '+name)
    for name, key in (('capture-script.py', 'capture_script_sha256'),
            ('capture-harness.c', 'harness_sha256'),
            ('observation-hooks.patch', 'instrumentation_sha256'),
            ('observation-helper.py', 'observation_helper_sha256'),
            ('evidence-validator.py', 'evidence_validator_sha256'),
            ('base-evidence-validator.py', 'base_evidence_validator_sha256')):
        if sha(files[name]) != report[key]:
            raise ValueError('Changed projection capture source '+name)
    log = gzip.decompress(files['capture.log.gz'])
    if COMPLETION not in log:
        raise ValueError('Missing successful Value Invert capture log marker')
    if check_runtime_diagnostics(log) != report['runtime_diagnostics']:
        raise ValueError('Projection diagnostics were not retained exactly')
    tests = json.loads(files['capture-plan.json'])
    check_plan(tests, full=True)
    records, proof = observations(files, tests)
    manifest = json.loads(files['fixtures.json'])
    if (records != manifest['cases'] or proof != manifest['proof'] or
        manifest['capture_columns'] != COLUMNS or manifest['copy_columns'] != COPY_COLUMNS or
        manifest['option_columns'] != OPTION_COLUMNS or len(records) != 16):
        raise ValueError('Projection index differs')
    if not all(proof.values()):
        raise ValueError('Full supplement lacks distinguishing cache/hidden-RGB witnesses')
    return report, records, proof


def verify(archive=DEFAULT, extract=None):
    files = read_archive(archive, 'legacy-value-invert-projection', ARCHIVE_SHA256,
                         MANIFEST_SHA256, 8*1024*1024, 1000, 8*1024*1024)
    report, records, proof = check_capture(files, REPORT_SHA256)
    if extract is not None:
        target = Path(extract)
        if target.is_symlink() or (target.exists() and any(target.iterdir())):
            raise ValueError('Nonempty projection target')
        target.mkdir(parents=True, exist_ok=True)
        for name, raw in files.items():
            (target/name).write_bytes(raw)
    return dict(source_commit=COMMIT, case_count=16, live_runs=48,
                source_layouts=[3, 4], output_layouts=[4], report_sha256=REPORT_SHA256, **proof)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--extract', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify(extract=args.extract), sort_keys=True))
