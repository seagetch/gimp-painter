#!/usr/bin/env python3
"""Seal the existing genuine context evidence without changing/removing it."""
import gzip
import hashlib
import json
from pathlib import Path
import tarfile

ROOT = Path(__file__).resolve().parents[1]


def bundle(roots, archive):
    manifest_path = archive.with_suffix(".manifest.json")
    if archive.exists() or manifest_path.exists():
        raise SystemExit("Bundle already sealed: " + str(archive))
    members = {}
    with archive.open("xb") as output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=output, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w", format=tarfile.PAX_FORMAT) as tar:
                for root in roots:
                    for path in sorted(root.rglob("*")):
                        if path.is_symlink():
                            raise ValueError("Evidence cannot contain symlinks")
                        if not path.is_file():
                            continue
                        name = root.name + "/" + path.relative_to(root).as_posix()
                        data = path.read_bytes()
                        entry = tarfile.TarInfo(name)
                        entry.size = len(data)
                        entry.mode = 0o644
                        with path.open("rb") as stream:
                            tar.addfile(entry, stream)
                        members[name] = dict(size=len(data), sha256=hashlib.sha256(data).hexdigest())
    manifest = dict(schema_version=1, archive=archive.name,
                    archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                    roots=[root.name for root in roots], member_count=len(members),
                    total_bytes=sum(member["size"] for member in members.values()), members=members)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(archive.relative_to(ROOT), len(members), manifest["total_bytes"], "bytes preserved")


if __name__ == "__main__":
    bundle([ROOT / "migration/fixtures/legacy-filter-context"],
           ROOT / "migration/fixtures/legacy-filter-context.tar.gz")
    bundle(sorted((ROOT / "migration/tests").glob("legacy-filter-context-initial-*")),
           ROOT / "migration/tests/legacy-filter-context-invalid-attempts.tar.gz")
