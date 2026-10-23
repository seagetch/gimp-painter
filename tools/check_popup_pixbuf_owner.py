#!/usr/bin/env python3
"""Check task 08.016/pixbuf-owner source duties and optional native TAP evidence."""
import argparse
import csv
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'migration/contracts/popup-pixbuf-owner.json'
SOURCES = [
    'app/widgets/gimpcellrendererpopup.cpp',
    'app/widgets/gimpcellrendererpopup.h',
    'app/widgets/gimpcellrendererpopup-handle.hpp',
    'app/tests/test-painter-popup-handles.cpp',
    'app/tests/meson.build',
    'migration/contracts/popup-pixbuf-owner.json',
    'migration/contracts/popup-pixbuf-owner.md',
    'tools/check_popup_pixbuf_owner.py',
    'tools/tests/test_popup_pixbuf_owner.py',
]
CASE_NAMES = {
    'initial-and-empty', 'replace-same-reset', 'recovery',
    'failed-factory-result', 'caller-reference-survives-renderer',
    'column-first', 'handle-first', 'retained-after-tree-close',
}
EXPECTED = {'/painter-popup-handles/pixbuf/' + name for name in CASE_NAMES}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def validate(manifest):
    assert manifest['task'] == '08.016/pixbuf-owner'
    assert manifest['runtime_required'] is True
    assert len(manifest['focused_cases']) == len(EXPECTED)
    assert set(manifest['focused_cases']) == EXPECTED
    with (ROOT / 'migration/inventory/legacy-port-work-items.tsv').open() as stream:
        rows = {row['work_id']: row for row in csv.DictReader(stream, delimiter='\t')
                if row['wbs_task'] == manifest['task']}
    duties = manifest['duties']
    assert len(duties) == len(rows) == 6
    assert {duty['work_id'] for duty in duties} == set(rows)
    legacy = {}
    for duty in duties:
        row = rows[duty['work_id']]
        for key in ('source_id', 'path', 'source_blob', 'source_range',
                    'source_sha256', 'source_scope'):
            assert duty[key] == row[key], (duty['work_id'], key)
        identity = manifest['legacy_commit'] + ':' + duty['path']
        blob = subprocess.check_output(['git', 'show', identity], cwd=ROOT)
        actual_id = subprocess.check_output(
            ['git', 'rev-parse', identity], cwd=ROOT, text=True).strip()
        assert actual_id == duty['source_blob']
        # source_sha256 identifies the inventoried diff hunk, not the full blob.
        assert sha256(blob) == duty['blob_sha256']
        assert duty['mapping'].strip()
        if duty['path'].endswith('gimpcellrendererpopup.cpp'):
            assert duty['role'] == 'direct-owner'
        elif duty['path'].endswith('gimpcellrendererpopup.h'):
            assert duty['role'] == 'owner-interface'
        else:
            assert duty['role'] == 'support-no-pixbuf-owner'
            assert 'GdkPixbuf' not in blob.decode()
        legacy[duty['path']] = blob.decode()
    old = legacy['app/widgets/gimpcellrendererpopup.cpp']
    for token in ('GdkPixbuf*  pixbuf;', 'gtk_widget_render_icon (widget,',
                  'gdk_pixbuf_get_width  (pixbuf)', 'g_object_unref (pixbuf);',
                  'gdk_cairo_set_source_pixbuf (cr, pixbuf,'):
        assert token in old

    source = (ROOT / SOURCES[0]).read_text()
    header = (ROOT / SOURCES[1]).read_text()
    tests = (ROOT / SOURCES[3]).read_text()
    assert 'GTK_TYPE_CELL_RENDERER_PIXBUF)' in source
    assert 'GtkCellRendererPixbuf parent_instance;' in header
    assert 'GtkCellRendererPixbufClass parent_class;' in header
    assert '"icon-name", GIMP_ICON_MENU_RIGHT' in source
    assert 'G_OBJECT_CLASS (gimp_cell_renderer_popup_parent_class)->dispose (object);' in source
    for retired in ('create_pixbuf', 'gdk_cairo_set_source_pixbuf', 'GdkPixbuf *', 'GdkPixbuf*'):
        assert retired not in source and retired not in header, retired
    for vfunc in ('render', 'get_size', 'get_preferred_width', 'get_preferred_height'):
        assert not re.search(r'->\s*' + vfunc + r'\s*=', source), vfunc
        assert f'klass->{vfunc} == parent->{vfunc}' in tests
    registrations = re.findall(r'g_test_add_func\s*\(\s*"([^"]+)"', tests)
    focused = [name for name in registrations if '/pixbuf/' in name]
    assert len(focused) == len(EXPECTED) and set(focused) == EXPECTED
    for token in ('gdk_pixbuf_new_from_data', 'release_pixels, &finalized',
                  'gtk_icon_theme_load_icon', 'GTK_ICON_THEME_NOT_FOUND',
                  'gtk_cell_renderer_render', 'g_assert_true (unchanged)',
                  'g_assert_false (unchanged)'):
        assert token in tests, token
    assert "test('painter-popup-handles', painter_popup_handles_test," in (
        ROOT / 'app/tests/meson.build').read_text()


def validate_tap(text):
    """Require all eight real focused cases exactly once and no failure/skip."""
    assert not re.search(r'(?m)^\s*not ok\b|^\s*Bail out!|#\s*SKIP\b', text, re.I)
    assert not re.search(r'ERROR: (?:AddressSanitizer|LeakSanitizer)|'
                         r'AddressSanitizer:DEADLYSIGNAL|runtime error:', text)
    passed = re.findall(r'^\s*ok\s+\d+\s+(/painter-popup-handles/[^\s#]+)\s*$',
                        text, re.M)
    plans = re.findall(r'^1\.\.(\d+)\s*$', text, re.M)
    assert len(plans) == 1 and int(plans[0]) == len(passed), plans
    focused = [name for name in passed if '/pixbuf/' in name]
    assert len(focused) == len(EXPECTED), focused
    assert set(focused) == EXPECTED, focused
    return {'status': 'PASS', 'focused_cases': sorted(focused),
            'focused_count': len(focused), 'registration_case_count': len(passed)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', action='append', default=[], metavar='LABEL=LOG',
                        help='Sanitized TAP stdout from a successful native run')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    validate(manifest)
    runs = []
    for item in args.run:
        label, filename = item.split('=', 1)
        raw = Path(filename).read_bytes()
        result = validate_tap(raw.decode())
        result.update(label=label, log_sha256=sha256(raw))
        runs.append(result)
    report = {
        'task': manifest['task'],
        'status': 'SOURCE_AND_NATIVE_TAP_PASS' if runs else 'SOURCE_CONTRACT_PASS',
        'source_duties': 6,
        'runs': runs,
        'limits': manifest['limits'],
        'source_sha256': {path: sha256((ROOT / path).read_bytes()) for path in SOURCES},
    }
    if args.report:
        args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
