#!/usr/bin/env python3
"""Validate a stroke-record v1, optionally verifying all referenced file hashes.

Schema/ordering/hash validity is not proof of a real legacy application run.
Runtime evidence must be reviewed with the recorded build and capture log.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[1]
SCHEMA = ROOT / "migration/fixtures/strokes/stroke-record.schema.json"


def walk(value):
    yield value
    if isinstance(value, dict):
        for child in value.values():
            yield from walk(child)
    elif isinstance(value, list):
        for child in value:
            yield from walk(child)


def validate(record, base=None):
    """Return errors; base enables bounded local sidecar/hash verification."""
    schema = json.loads(SCHEMA.read_text())
    errors = [f"{'/'.join(map(str, error.absolute_path))}: {error.message}"
              for error in jsonschema.Draft202012Validator(schema).iter_errors(record)]
    if errors:
        return errors
    if any(isinstance(value, float) and not math.isfinite(value) for value in walk(record)):
        errors.append("non-finite numeric value")
    events = record["events"]
    if [event["sequence"] for event in events] != list(range(len(events))):
        errors.append("event sequence must be contiguous and start at zero")
    if any(a["monotonic_ns"] > b["monotonic_ns"] for a, b in zip(events, events[1:])):
        errors.append("monotonic timestamps must not decrease")
    if events[0]["phase"] != "begin" or events[-1]["phase"] != "end":
        errors.append("complete stroke must begin and end explicitly")
    if any(event["phase"] != "motion" for event in events[1:-1]):
        errors.append("interior events must have motion phase")
    if len(record["engine_state"]["state_names"]) != len(record["engine_state"]["state_f32_bits"]):
        errors.append("engine state names and bit-pattern counts differ")
    if any(result["checkpoint_sequence"] >= len(events) for result in record["results"]):
        errors.append("result refers to an absent event")
    for value in walk(record):
        if not isinstance(value, dict) or set(value) != {"path", "sha256"}:
            continue
        path = Path(value["path"])
        if path.is_absolute() or ".." in path.parts or "\\" in value["path"] or ":" in value["path"]:
            errors.append(f"sidecar path must be a portable relative path: {path}")
            continue
        if base is not None:
            base = Path(base).resolve()
            target = (base / path).resolve()
            if not target.is_relative_to(base):
                errors.append(f"sidecar escapes record directory: {path}")
            elif not target.is_file():
                errors.append(f"missing sidecar: {path}")
            elif hashlib.sha256(target.read_bytes()).hexdigest() != value["sha256"]:
                errors.append(f"sidecar hash mismatch: {path}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("record", type=Path)
    parser.add_argument("--verify-files", action="store_true")
    args = parser.parse_args()
    record = json.loads(args.record.read_text())
    errors = validate(record, args.record.parent if args.verify_files else None)
    if errors:
        parser.exit(1, "\n".join(errors) + "\n")
    print(f"valid {record['evidence_kind']} format; runtime provenance still requires review")


if __name__ == "__main__":
    main()
