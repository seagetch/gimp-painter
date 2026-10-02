#!/usr/bin/env python3
"""Lock source assets without claiming that the legacy application ran.

Default mode verifies the committed lock against Git blobs at the pinned source.
--write regenerates only lock metadata. --materialize copies exact asset bytes to
an empty, caller-selected directory for local tests (it never overwrites files).
"""

import argparse
import collections
import csv
import hashlib
import io
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REV = "afa43fae3e920210146abed514f136fd49f671b5"
DEST = ROOT / "migration/fixtures/legacy-source"
FIELDS = ("path", "git_blob", "sha256", "bytes", "kind", "myb_version", "license_evidence")
SOURCE_PATHS = ("data/mypaint-brushes", "data/layer-presets", "LICENSE", "COPYING")


def git(*args):
    return subprocess.check_output(["git", "-C", str(ROOT), *args])


def blobs():
    entries = []
    for line in git("ls-tree", "-r", "-z", REV, "--", *SOURCE_PATHS).split(b"\0"):
        if not line:
            continue
        info, path = line.split(b"\t", 1)
        mode, kind, oid = info.decode().split()
        if mode != "100644" and mode != "100755" or kind != "blob":
            raise ValueError(f"unsupported source asset mode: {line!r}")
        entries.append((path.decode("utf-8"), oid))
    # One batch avoids per-asset lazy fetches in a partial clone.
    requests = "".join(oid + "\n" for _, oid in entries).encode()
    result = io.BytesIO(subprocess.check_output(
        ["git", "-C", str(ROOT), "cat-file", "--batch"], input=requests))
    output = []
    for path, oid in entries:
        header = result.readline().decode().split()
        if len(header) != 3 or header[:2] != [oid, "blob"]:
            raise ValueError(f"missing pinned blob {path}: {header}")
        data = result.read(int(header[2]))
        if len(data) != int(header[2]) or result.read(1) != b"\n":
            raise ValueError(f"truncated Git response: {path}")
        output.append((path, oid, data))
    return sorted(output)


def asset_record(path, oid, data):
    suffix = Path(path).suffix
    version = "-"
    kind = "support"
    if suffix == ".myb":
        kind = "brush"
        text = data.decode("utf-8")
        if text.lstrip().startswith("{"):
            version = str(json.loads(text)["version"])
        else:
            match = re.search(r"^version\s+(\d+)\s*$", text, re.MULTILINE)
            if not match:
                raise ValueError(f"brush has no version: {path}")
            version = match[1]
    elif suffix == ".png":
        kind = "preview"
    elif path.startswith("data/layer-presets/") and suffix == ".json":
        kind = "layer-preset"
    elif path in ("COPYING", "LICENSE") or suffix == ".txt":
        kind = "notice"
    if path.startswith("data/mypaint-brushes/deevad/"):
        license_evidence = "data/mypaint-brushes/deevad/readme.txt"
    elif path.startswith("data/mypaint-brushes/kaerhon/"):
        license_evidence = "data/mypaint-brushes/kaerhon/ReadMe.txt"
    else:
        license_evidence = "LICENSE;COPYING"
    return dict(zip(FIELDS, (path, oid, hashlib.sha256(data).hexdigest(),
                            str(len(data)), kind, version, license_evidence)))


def render(source):
    rows = [asset_record(*entry) for entry in source]
    counts = collections.Counter(row["myb_version"] for row in rows if row["kind"] == "brush")
    if counts != {"2": 1, "3": 176}:
        raise ValueError(f"pinned brush census changed: {counts}")
    tsv = io.StringIO(newline="")
    writer = csv.DictWriter(tsv, fieldnames=FIELDS, delimiter="\t", lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    manifest = tsv.getvalue()
    summary = {
        "schema_version": 1,
        "evidence_kind": "pinned-source-assets-not-runtime-captures",
        "repository": "https://github.com/seagetch/gimp-painter.git",
        "commit": REV,
        "source_paths": list(SOURCE_PATHS),
        "asset_count": len(rows),
        "brush_count": sum(counts.values()),
        "brush_versions": dict(sorted(counts.items())),
        "kind_counts": dict(sorted(collections.Counter(row["kind"] for row in rows).items())),
        "manifest_sha256": hashlib.sha256(manifest.encode()).hexdigest(),
    }
    return manifest, json.dumps(summary, indent=2) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--materialize", type=Path)
    args = parser.parse_args()
    source = blobs()
    manifest, summary = render(source)
    files = {DEST / "assets.tsv": manifest, DEST / "manifest.json": summary}
    if args.write:
        DEST.mkdir(parents=True, exist_ok=True)
        for path, text in files.items():
            path.write_text(text, encoding="utf-8")
    else:
        for path, text in files.items():
            if not path.exists() or path.read_text(encoding="utf-8") != text:
                parser.error(f"lock differs from pinned source: {path}")
    if args.materialize:
        target = args.materialize.resolve()
        if target.exists() and any(target.iterdir()):
            parser.error("materialization directory must be empty")
        target.mkdir(parents=True, exist_ok=True)
        for path, _, data in source:
            destination = target / path
            destination.parent.mkdir(parents=True, exist_ok=True)
            with destination.open("xb") as output:
                output.write(data)
    print(f"verified {len(source)} source assets; 177 brushes (176 v3, 1 v2); no runtime claim")


if __name__ == "__main__":
    main()
