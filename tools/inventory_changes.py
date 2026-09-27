#!/usr/bin/env python3
"""Produce the exhaustive legacy file-change inventory from Git objects."""

import argparse
import csv
import hashlib
import json
import subprocess
from pathlib import Path


def git(*args: str) -> bytes:
    return subprocess.check_output(["git", *args])


def tree(revision: str) -> dict[bytes, str]:
    blobs = {}
    for entry in git("ls-tree", "-rz", "--full-tree", revision).split(b"\0"):
        if not entry:
            continue
        metadata, name = entry.split(b"\t", 1)
        _mode, _kind, oid = metadata.decode("ascii").split(" ")
        blobs[name] = oid
    return blobs


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    base = git("rev-parse", args.base).decode().strip()
    source = git("rev-parse", args.source).decode().strip()
    before, after = tree(base), tree(source)
    changes = git("diff", "--no-renames", "--name-status", "-z", base, source).split(b"\0")
    rows = []
    for index in range(0, len(changes) - 1, 2):
        status, path = changes[index:index + 2]
        rows.append((path.decode("utf-8", "surrogateescape"), status.decode(),
                     before.get(path, ""), after.get(path, "")))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("path", "change", "base_git_blob", "source_git_blob"))
        writer.writerows(rows)
    payload = args.output.read_bytes()
    manifest = {"base": base, "source": source, "files": len(rows),
                "git_blob_hash": "SHA-1", "inventory_sha256": hashlib.sha256(payload).hexdigest()}
    args.output.with_suffix(".json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(rows)} changes; manifest {args.output.with_suffix('.json')}")


if __name__ == "__main__":
    main()
