#!/usr/bin/env python3
"""Compare every inventoried legacy hunk with both fixed GIMP 3 baselines.

Exact text is a source witness, never a behavioral-equivalence decision.
--check verifies committed output without rewriting it.
"""
import argparse
import csv
import hashlib
import io
import json
import re
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path('migration/inventory')
FIELDS = ('child_id', 'status', 'hunk_id', 'path', 'hunk', 'target_role',
          'target_commit', 'source_blob', 'target_blob', 'added_sha256',
          'removed_sha256', 'source_relation', 'target_added_line',
          'target_removed_line', 'behavior_relation', 'disposition')


def git(*args):
    return subprocess.check_output(['git', *args])


def digest(data):
    return hashlib.sha256(data).hexdigest()


def normalized(data):
    # Preserve tokens, comments, line order, and line boundaries.
    return '\n'.join(re.sub(r'\s+', ' ', line).strip()
                     for line in data.decode('utf-8', 'replace').splitlines()
                     if line.strip())


def locate(needle, haystack):
    if not needle:
        return ''
    offset = ('\n' + haystack + '\n').find('\n' + needle + '\n')
    if offset < 0:
        return ''
    # This is a normalized (nonblank) line, not an original source line.
    return str(haystack.count('\n', 0, offset) + 1)


def render():
    baseline = json.loads(Path('migration/baseline/baseline.json').read_text())
    base = baseline['source_diff_base']['commit']
    source = baseline['source']['commit']
    targets = [('published', baseline['published_comparison']['commit']),
               ('initial-port', baseline['initial_port']['upstream_commit'])]
    patch = git('-c', 'core.quotePath=false', 'diff', '--no-renames',
                '--no-ext-diff', '--no-textconv', '--unified=0', base, source)
    payloads = {}
    path, index = '', 0
    for line in patch.splitlines(keepends=True):
        if line.startswith(b'diff --git a/'):
            path = line[len(b'diff --git a/'):].split(b' b/', 1)[0].decode()
            index = 0
        elif line.startswith(b'@@ '):
            index += 1
            payloads[(path, str(index))] = [bytearray(), bytearray()]
        elif index and line[:1] in (b'+', b'-') and not line.startswith((b'+++', b'---')):
            payloads[(path, str(index))][line[:1] == b'-'].extend(line[1:])
    with (ROOT / 'changed-hunks.tsv').open() as f:
        hunks = list(csv.DictReader(f, delimiter='\t'))
    with (ROOT / 'changed-files.tsv').open() as f:
        files = {r['path']: r for r in csv.DictReader(f, delimiter='\t')}
    if set(payloads) != {(r['path'], r['index']) for r in hunks if r['kind'] == 'text'}:
        raise ValueError('diff and inventoried hunk keys disagree')
    target_trees = {}
    contents = {}
    for role, commit in targets:
        tree = {}
        for entry in git('ls-tree', '-rz', commit).split(b'\0'):
            if not entry:
                continue
            metadata, p = entry.split(b'\t', 1)
            tree[p.decode()] = metadata.split()[2].decode()
        target_trees[role] = tree
    out = io.StringIO()
    writer = csv.writer(out, delimiter='\t', lineterminator='\n')
    writer.writerow(FIELDS)
    for row in hunks:
        p = row['path']
        added, removed = payloads.get((p, row['index']), (b'', b''))
        source_blob = files[p]['source_git_blob']
        for role, commit in targets:
            target_blob = target_trees[role].get(p, '')
            added_line = removed_line = ''
            if source_blob and source_blob == target_blob:
                relation = 'IDENTICAL_FILE'
            elif not target_blob:
                relation = 'PATH_ABSENT'
            elif row['kind'] != 'text':
                relation = 'DIFFERENT_FILE_METADATA_OR_BINARY'
            else:
                if target_blob not in contents:
                    contents[target_blob] = normalized(git('show', target_blob))
                text = contents[target_blob]
                added_line = locate(normalized(added), text)
                removed_line = locate(normalized(removed), text)
                if added_line:
                    relation = 'ADDED_TEXT_PRESENT'
                elif not normalized(added):
                    relation = 'DELETION_ONLY_REQUIRES_REVIEW'
                else:
                    relation = 'ADDED_TEXT_NOT_FOUND'
            writer.writerow((row['child_id'].replace('01.002/', '01.012/') + '-' + role,
                             'DONE', row['child_id'], p, row['old_new_lines'], role,
                             commit, source_blob, target_blob, digest(added), digest(removed),
                             relation, added_line, removed_line, 'NOT_PROVEN',
                             'retain-for-01.013; no deletion authorized by text matching'))
    return out.getvalue()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    path = ROOT / 'upstream-hunk-comparison.tsv'
    result = render()
    if args.check:
        if path.read_text() != result:
            raise SystemExit('upstream hunk inventory differs; regenerate and review')
    else:
        path.write_text(result)
    rows = list(csv.DictReader(io.StringIO(result), delimiter='\t'))
    print(f'{len(rows)} comparisons; {len(rows)//2} hunks; ' +
          json.dumps(dict(Counter(r['source_relation'] for r in rows)), sort_keys=True))


if __name__ == '__main__':
    main()
