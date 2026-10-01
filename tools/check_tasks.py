#!/usr/bin/env python3
"""Validate the migration WBS, dependency graph, and completion evidence."""

import argparse
import csv
import json
import re
import sys
from pathlib import Path


ROW = re.compile(r"^\|\s*(\d{2}\.\d{3}(?:/[A-Za-z0-9_.-]+)?)\s*\|\s*\[([ x])\]\s*\|.*?\|\s*([^|]+)\|\s*[^|]+\|$")
ID = re.compile(r"\d{2}\.\d{3}(?:/[A-Za-z0-9_.-]+)?")
FIELDS = ("id", "status", "owner", "commit_or_artifact", "test", "result", "added_children", "limitations")


def validate(tasks_path: Path, evidence_path: Path) -> list[str]:
    text = tasks_path.read_text(encoding="utf-8")
    wbs = text.split("## 4. WBS\n", 1)
    if len(wbs) != 2:
        return ["missing WBS section"]
    section = wbs[1].split("## 5. ", 1)[0]
    rows = {}
    errors = []
    for line in section.splitlines():
        match = ROW.match(line)
        if not match:
            if re.match(r"^\|\s*\d{2}\.\d{3}", line):
                errors.append(f"malformed task row: {line[:100]}")
            continue
        task, checked, deps = match.groups()
        if task in rows:
            errors.append(f"duplicate task: {task}")
        rows[task] = (checked == "x", set(ID.findall(deps)))
    if not rows:
        errors.append("no tasks found")

    for task, (checked, deps) in rows.items():
        for dep in deps:
            if dep not in rows:
                errors.append(f"{task}: undefined dependency {dep}")
            elif checked and not rows[dep][0]:
                errors.append(f"{task}: checked before dependency {dep}")

    visiting, visited = set(), set()

    def visit(task: str) -> None:
        if task in visiting:
            errors.append(f"dependency cycle at {task}")
            return
        if task in visited:
            return
        visiting.add(task)
        for dep in rows[task][1]:
            if dep in rows:
                visit(dep)
        visiting.remove(task)
        visited.add(task)

    for task in rows:
        visit(task)

    with evidence_path.open(newline="", encoding="utf-8") as file:
        reader = csv.DictReader(file, delimiter="\t")
        if tuple(reader.fieldnames or ()) != FIELDS:
            errors.append("progress.tsv has incorrect columns")
        evidence = {}
        for record in reader:
            task = record["id"]
            if task in evidence:
                errors.append(f"duplicate evidence: {task}")
            evidence[task] = record
            if task not in rows:
                errors.append(f"evidence for unknown task: {task}")
            if record["status"] not in ("TODO", "DOING", "BLOCKED", "DONE"):
                errors.append(f"{task}: invalid status")
    for task, (checked, _) in rows.items():
        record = evidence.get(task)
        if checked and (not record or record["status"] != "DONE"):
            errors.append(f"{task}: checked without DONE evidence")
        if record and (record["status"] == "DONE") != checked:
            errors.append(f"{task}: evidence and checkbox disagree")
        if checked and (not record["commit_or_artifact"] or not record["test"] or not record["result"]):
            errors.append(f"{task}: incomplete evidence")
    for task, filename, count in (
            ("01.002", "changed-hunks.tsv", 2489),
            ("01.003", "external-changes.tsv", 474),
            ("01.004", "cpp-types.tsv", 17)):
        if task not in rows or not rows[task][0]:
            continue
        child_path = evidence_path.parent / "inventory" / filename
        if not child_path.exists():
            errors.append(f"{task}: missing child checklist {child_path}")
            continue
        with child_path.open(newline="", encoding="utf-8") as file:
            children = list(csv.DictReader(file, delimiter="\t"))
        if len(children) != count or any(
                child.get("status") != "DONE" or not child.get("child_id", "").startswith(task + "/")
                for child in children):
            errors.append(f"{task}: incomplete or malformed child checklist")
    if rows.get('01.012', (False,))[0]:
        inventory = evidence_path.parent / 'inventory'
        paths = [inventory / name for name in
                 ('changed-hunks.tsv', 'upstream-hunk-comparison.tsv',
                  'upstream-contracts.json', 'upstream-contract-review.tsv')]
        if any(not path.exists() for path in paths):
            errors.append('01.012: missing upstream comparison child ledger')
        else:
            def read_tsv(path):
                with path.open(newline='', encoding='utf-8') as file:
                    return list(csv.DictReader(file, delimiter='\t'))
            hunk_ids = {r['child_id'] for r in read_tsv(paths[0])}
            comparisons = read_tsv(paths[1])
            baseline = json.loads((evidence_path.parent / 'baseline' / 'baseline.json').read_text())
            targets = {'published': baseline['published_comparison']['commit'],
                       'initial-port': baseline['initial_port']['upstream_commit']}
            expected = {(hunk, role, commit) for hunk in hunk_ids
                        for role, commit in targets.items()}
            actual = {(r['hunk_id'], r['target_role'], r['target_commit']) for r in comparisons}
            if (actual != expected or len(comparisons) != len(expected) or
                    len({r['child_id'] for r in comparisons}) != len(comparisons) or
                    any(r['status'] != 'DONE' or r['behavior_relation'] != 'NOT_PROVEN'
                        for r in comparisons)):
                errors.append('01.012: incomplete or overclaimed upstream hunk checklist')
            manifest = json.loads(paths[2].read_text())
            reviews = read_tsv(paths[3])
            expected_reviews = {f'01.012/{r["key"]}-{role}' for r in manifest for role in targets}
            if ({r['child_id'] for r in reviews} != expected_reviews or
                    len(reviews) != len(expected_reviews) or
                    any(r['status'] != 'DONE' or not r['legacy_site'] or not r['target_site']
                        or not r['behavior_evidence'] or not r['followup'] for r in reviews)):
                errors.append('01.012: incomplete upstream contract checklist')
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tasks", type=Path, default=Path("tasks.md"))
    parser.add_argument("--evidence", type=Path, default=Path("migration/progress.tsv"))
    args = parser.parse_args()
    errors = validate(args.tasks, args.evidence)
    for error in errors:
        print(error, file=sys.stderr)
    if errors:
        return 1
    print("WBS dependencies, states, and completion evidence: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
