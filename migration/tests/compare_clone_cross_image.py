#!/usr/bin/env python3
"""Compare every recorded ordinary RGB legacy cross-image observation to the port."""
import argparse
import hashlib
import json
from pathlib import Path
from test_legacy_clone_cross_image_fixtures import parsed_records

parser = argparse.ArgumentParser()
parser.add_argument("--log", type=Path, default=Path("migration/tests/clone-layer-testlog.json"))
parser.add_argument("--sanitizer-report", type=Path)
parser.add_argument("--report", type=Path, default=Path("migration/tests/clone-cross-image-comparison.json"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
fixture = root / "migration/fixtures/legacy-clone-cross-image/observations.json"
rows = [json.loads(line) for line in args.log.read_text().splitlines() if line]
assert len(rows) == 1 and rows[0]["result"] == "OK"
stdout = "\n".join(line.removeprefix("# ") for line in rows[0]["stdout"].splitlines())
actual = parsed_records(stdout)
expected = json.loads(fixture.read_text())["records"]
report = {"scope": "ordinary RGB direct/group cross-image copy and source relocation",
          "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
          "port_log_sha256": hashlib.sha256(args.log.read_bytes()).hexdigest(),
          "expected_records": len(expected), "actual_records": len(actual),
          "exact_match": actual == expected, "port_records": actual,
          "limits": ["Identity predicates and solid-fill pixel/geometry records only",
                     "Legacy teardown warnings are a separately documented safety repair"]}
if args.sanitizer_report:
    sanitized = json.loads(args.sanitizer_report.read_text())
    assert sanitized["exit_code"] == 0
    text = "\n".join(line.removeprefix("# ") for line in sanitized["stdout"].splitlines())
    report["sanitizer_trace_exact_match"] = parsed_records(text) == expected
    report["sanitizer_report_sha256"] = hashlib.sha256(args.sanitizer_report.read_bytes()).hexdigest()
args.report.write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({key: report[key] for key in ("expected_records", "actual_records", "exact_match")}))
raise SystemExit(0 if actual == expected and report.get("sanitizer_trace_exact_match", True) else 1)
