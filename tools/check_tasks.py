#!/usr/bin/env python3
"""Validate the migration WBS, dependency graph, and completion evidence."""

import argparse
import csv
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
