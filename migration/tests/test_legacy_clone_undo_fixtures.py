"""Check the sealed, real legacy RGB resize/Undo observation package."""

import hashlib
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1] / "fixtures" / "legacy-clone-undo"


def parsed_records(text):
    records = []
    for line in text.splitlines():
        if not line.startswith(("UNDO_STATE ", "UNDO_NODE ", "UNDO_ACTION ")):
            continue
        kind, *tokens = line.split()
        row = {"record": kind}
        for token in tokens:
            key, value = token.split("=", 1)
            if key.endswith(("_wh", "_xy")):
                value = [int(part) for part in value.split(",")]
            elif value.lstrip("-").isdigit() and key != "path":
                value = int(value)
            row[key] = value
        records.append(row)
    return records


class LegacyCloneUndoFixtures(unittest.TestCase):
    def test_sealed_file_hashes_and_build_provenance(self):
        manifest = json.loads((ROOT / "manifest.json").read_text())
        self.assertEqual(
            set(manifest["files"]),
            {p.name for p in ROOT.iterdir() if p.name != "manifest.json"},
        )
        for name, expected in manifest["files"].items():
            with self.subTest(file=name):
                data = (ROOT / name).read_bytes()
                self.assertEqual(len(data), expected["bytes"])
                self.assertEqual(hashlib.sha256(data).hexdigest(), expected["sha256"])
        report = json.loads((ROOT / "capture-report.json").read_text())
        build_report = (ROOT / report["build_report"]).read_bytes()
        self.assertEqual(hashlib.sha256(build_report).hexdigest(), report["build_report_sha256"])
        self.assertEqual(report["source_commit"], "afa43fae3e920210146abed514f136fd49f671b5")
        self.assertEqual(report["capture_exit_code"], 0)
        self.assertEqual(report["build_exit_code"], 0)

    def test_machine_observations_match_raw_log(self):
        observations = json.loads((ROOT / "observations.json").read_text())
        self.assertEqual(observations["records"], parsed_records((ROOT / "capture.log").read_text()))

    def test_both_scenes_and_full_two_cycle_trace_are_present(self):
        log = (ROOT / "capture.log").read_text()
        records = parsed_records(log)
        steps = ["baseline", "resized", "undo1", "redo1", "undo2", "redo2"]
        for scene in ("no-mask", "white-mask"):
            states = [r for r in records if r["record"] == "UNDO_STATE" and r["case"] == scene]
            self.assertEqual([r["step"] for r in states], steps)
            self.assertEqual([r["clone_xy"] for r in states], [[32, 30], [34, 33], [30, 27], [34, 33], [30, 27], [34, 33]])
            self.assertEqual([r["undo_depth"] for r in states], [0, 1, 2, 4, 4, 6])
            self.assertEqual([r["redo_depth"] for r in states], [0, 0, 1, 0, 1, 0])
            self.assertTrue(all(r["group_count"] == 0 for r in states))
            if scene == "white-mask":
                self.assertTrue(all(r["mask_wh"] == [16, 16] and r["mask_xy"] == [32, 30] for r in states))
            actions = [r for r in records if r["record"] == "UNDO_ACTION" and r["case"] == scene]
            self.assertEqual([r["step"] for r in actions], steps[2:])
            self.assertTrue(all(r["returned"] == 1 for r in actions))
            self.assertIn("CLONE_RESIZE_UNDO_CASE_COMPLETE case=" + scene, log)
        self.assertEqual(log.count("gimp_item_resize: assertion 'GIMP_IS_CONTEXT (context)' failed"), 6)
        self.assertIn("CLONE_RESIZE_UNDO_CAPTURE_COMPLETE", log)


if __name__ == "__main__":
    unittest.main()
