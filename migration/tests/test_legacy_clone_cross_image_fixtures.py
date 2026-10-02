"""Verify the sealed genuine RGB cross-image CloneLayer reference capture."""
import hashlib
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1] / "fixtures" / "legacy-clone-cross-image"


def parsed_records(text):
    records = []
    for line in text.splitlines():
        if not line.startswith(("CROSS_REF ", "CROSS_PIXEL ")):
            continue
        kind, *tokens = line.split()
        row = {"record": kind}
        for token in tokens:
            key, value = token.split("=", 1)
            if key in ("rgba", "wh", "xy"):
                value = [int(part) for part in value.split(",")]
            elif value.isdigit():
                value = int(value)
            row[key] = value
        records.append(row)
    return records


class LegacyCloneCrossImageFixtures(unittest.TestCase):
    def test_sealed_file_hashes_and_provenance(self):
        manifest = json.loads((ROOT / "manifest.json").read_text())
        self.assertEqual(set(manifest["files"]),
                         {p.name for p in ROOT.iterdir() if p.name != "manifest.json"})
        for name, expected in manifest["files"].items():
            with self.subTest(file=name):
                data = (ROOT / name).read_bytes()
                self.assertEqual(len(data), expected["bytes"])
                self.assertEqual(hashlib.sha256(data).hexdigest(), expected["sha256"])
        report = json.loads((ROOT / "capture-report.json").read_text())
        self.assertEqual(report["source_commit"], "afa43fae3e920210146abed514f136fd49f671b5")
        self.assertEqual(report["build_exit_code"], 0)
        self.assertEqual(report["capture_exit_code"], 0)
        data = (ROOT / report["build_report"]).read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), report["build_report_sha256"])
        overlay = (ROOT / "source-overlay.patch").read_text()
        for path in report["unmodified_behavior_source_hashes"]:
            self.assertNotIn("diff --git a/" + path + " b/", overlay)

    def test_observations_match_raw_log(self):
        observations = json.loads((ROOT / "observations.json").read_text())
        self.assertEqual(observations["records"], parsed_records((ROOT / "capture.log").read_text()))

    def test_identity_pixel_geometry_and_warning_trace(self):
        log = (ROOT / "capture.log").read_text()
        records = parsed_records(log)
        refs = [r for r in records if r["record"] == "CROSS_REF"]
        self.assertEqual([r["step"] for r in refs],
                         ["direct-copy", "group-copy", "source-relocated", "original-closed"])
        self.assertTrue(all(value == 1 for r in refs for key, value in r.items()
                            if key not in ("record", "step")))
        pixels = [r for r in records if r["record"] == "CROSS_PIXEL"]
        self.assertEqual(len(pixels), 15)
        expected = [
            [204, 51, 102, 128], [255, 0, 0, 255], [255, 0, 0, 255],
            [0, 0, 255, 255], [255, 0, 0, 255], [255, 255, 0, 255],
            [0, 0, 255, 255], [255, 255, 0, 255], [0, 255, 255, 255],
            [0, 255, 0, 255], [0, 255, 0, 255], [0, 255, 0, 255],
            [255, 0, 255, 255], [255, 0, 255, 255], [0, 255, 255, 255],
        ]
        self.assertEqual([r["rgba"] for r in pixels], expected)
        self.assertTrue(all(r["wh"] == [16, 16] for r in pixels))
        self.assertEqual([r["xy"] for r in pixels],
                         [[32, 30]] * 3 + [[0, 0]] * 6 + [[32, 30]] * 2 +
                         [[0, 0], [32, 30], [0, 0], [0, 0]])
        self.assertEqual(log.count("g_value_set_object: assertion 'G_VALUE_HOLDS_OBJECT (value)' failed"), 2)
        self.assertEqual(log.count("gimp_drawable_project_region: assertion 'gimp_item_is_attached (GIMP_ITEM (drawable))' failed"), 1)
        self.assertEqual(log.count("instance with invalid (NULL) class pointer"), 4)
        self.assertEqual(log.count("g_signal_handler_find: assertion 'G_TYPE_CHECK_INSTANCE (instance)' failed"), 4)
        self.assertIn("CLONE_CROSS_IMAGE_CAPTURE_COMPLETE", log)


if __name__ == "__main__":
    unittest.main()
