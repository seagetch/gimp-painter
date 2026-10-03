#!/usr/bin/env python3
"""Materialize verified immutable context evidence into an isolated directory."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile


def materialize(archive, manifest_path, destination):
    archive, manifest_path, destination = map(Path, (archive, manifest_path, destination))
    manifest = json.loads(manifest_path.read_text())
    if (manifest.get("schema_version") != 1 or manifest.get("archive") != archive.name or
            hashlib.sha256(archive.read_bytes()).hexdigest() != manifest.get("archive_sha256")):
        raise ValueError("Invalid or changed context fixture archive")
    expected = manifest["members"]
    if (not isinstance(expected, dict) or len(expected) != manifest["member_count"] or
            len(expected) > 10000 or manifest["total_bytes"] > 64 * 1024 * 1024):
        raise ValueError("Unbounded context fixture manifest")
    destination.mkdir(parents=True, exist_ok=True)
    if any(destination.iterdir()):
        raise ValueError("Context fixtures require an empty isolated destination")
    seen, total = set(), 0
    with tarfile.open(archive, "r:gz") as tar:
        for member in tar:
            path = PurePosixPath(member.name)
            if (not member.isfile() or member.name in seen or member.name not in expected or
                    path.is_absolute() or any(part in ("", ".", "..") for part in member.name.split("/")) or
                    len(member.name) > 512 or member.size < 0 or member.size > 16 * 1024 * 1024):
                raise ValueError("Invalid context fixture archive member")
            item = expected[member.name]
            if member.size != item["size"]:
                raise ValueError("Context fixture member size changed")
            total += member.size
            if total > 64 * 1024 * 1024:
                raise ValueError("Context fixture archive exceeds its bound")
            stream = tar.extractfile(member)
            if stream is None:
                raise ValueError("Unreadable context fixture archive member")
            data = stream.read(member.size + 1)
            if len(data) != member.size or hashlib.sha256(data).hexdigest() != item["sha256"]:
                raise ValueError("Context fixture bytes changed")
            target = destination.joinpath(*path.parts)
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as output:
                output.write(data)
            seen.add(member.name)
    if seen != set(expected) or total != manifest["total_bytes"]:
        raise ValueError("Incomplete context fixture archive")
    return manifest
