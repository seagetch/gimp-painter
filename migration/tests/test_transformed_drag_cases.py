#!/usr/bin/env python3
"""Bounded source/fixture checks only; never build or launch GIMP/old captures."""
import ast
import copy
from fractions import Fraction
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
CASES = ROOT / "migration/tests/transformed-drag-cases.json"
EVENT = "app/tests/test-painter-navigation-events.c"
BUILDER = "migration/tests/build_navigation_event_sanitizers.py"
NEW_NAMES = ["radial-transformed", "radial-clamp", "pan-transformed", "device-image-origin"]


def check(condition, message):
    if not condition:
        raise ValueError(message)


def near(actual, expected):
    check(math.isclose(actual, expected, rel_tol=0, abs_tol=1e-8),
          f"literal mismatch: {actual!r} != {expected!r}")


def digest(data, expected):
    check(hashlib.sha256(data).hexdigest() == expected, "SHA-256 mismatch")


def number(value):
    return float(Fraction(str(value)))


def radial(case, start_key, expected_key):
    anchor = 300 * case["scale"] - math.hypot(*case[start_key])
    near(anchor, case["anchor"])
    for point, expected in zip(case["motions_relative_to_C"], case[expected_key], strict=True):
        near((math.hypot(*point) + anchor) / 300, number(expected))


def forward(point, angle, mirror, offset, scale=1.5):
    # Literal modern R*F geometry, independent of shell matrices/helpers.
    c, s = angle["cos"], angle["sin"]
    k = math.floor(256 * scale) // 2
    x = (-1 if mirror else 1) * (scale * point[0] - k)
    y = scale * point[1] - k
    return [k - offset[0] + c*x - s*y, k - offset[1] + s*x + c*y]


def inverse(point, angle, mirror, offset):
    c, s = angle["cos"], angle["sin"]
    x, y = point[0] - 192 + offset[0], point[1] - 192 + offset[1]
    return [(192 + (-1 if mirror else 1)*(c*x + s*y))/1.5,
            (192 - s*x + c*y)/1.5]


def limits(origin, size, viewport):
    # int() is C gint truncation toward zero, including negative limits.
    if viewport < size:
        return int(origin - viewport*.5), int(origin + size - viewport*.5)
    overpan = int(viewport - size*.5)
    return origin - overpan, origin + size - viewport + overpan


def validate_literals(data):
    check(data["schema_version"] == 1, "schema")
    check(data["inventory"] == {
        "new_groups": 4, "main_drags": 16, "stationary_reentries": 16,
        "primary_probe_clicks": 6, "normal_event": 31, "normal_ui": 4,
        "normal_pure": 6, "normal_total": 41, "focused_event_asan_ubsan": 31}, "inventory")
    check([g["name"] for g in data["groups"]] ==
          ["/navigation-events/drag/" + name for name in NEW_NAMES], "groups")
    check([g["sequences"] for g in data["groups"]] == [6, 2, 6, 2], "sequences")
    constants = data["constants"]
    check(constants["trig"] == {
        "0": {"cos": 1, "sin": 0},
        "37": {"cos": 0.7986355100472928, "sin": 0.6018150231520483},
        "90": {"cos": 0, "sin": 1}}, "frozen trigonometry")
    check(constants["zoom_unit"] == 300 and
          constants["zoom_limits"] == ["1/256", "256"], "zoom constants")
    check(constants["pan_scale"] == 1.5 and constants["image_size"] == [256, 256], "geometry setup")
    check(constants["pan_center_relative_to_C"] == [-13, 9] and
          constants["minimum_clamp_margin"] == 16 and
          constants["overpan_factor"] == .5, "origin/clamp constants")
    check(constants["tolerances"] == {"factor_and_coordinate": 1e-8,
          "angle_and_linear_matrix": 1e-10, "center_display_pixels": 1}, "tolerances")
    radial_group, clamp_group, pan, origin = data["groups"]
    radial(radial_group, "initial_start_relative_to_C", "requested_expected")
    for case in clamp_group["cases"]:
        radial(case, "start_relative_to_C", "requested_expected")
        for raw, displayed in zip(case["requested_expected"], case["displayed_expected"], strict=True):
            near(min(256, max(1/256, number(raw))), number(displayed))
    check(pan["motions_relative_to_start"] == [[0, 0], [17, -11], [17, -11], [-9, 7], [0, 0]], "pan sequence")
    for pointer, offset in zip(pan["motions_relative_to_start"], pan["expected_offsets_relative_to_O0"], strict=True):
        check(offset == [-pointer[0], -pointer[1]], "fixed pan origin/stationary drift")
    check(origin["expected_pan_offset_delta"] == [-17, 11] and
          origin["start_relative_to_C"] == [31.25, -18.75], "precise origin")
    check([(g["angle"], g["mirror"]) for g in data["literal_geometry"]] ==
          [(a, m) for a in (0, 37, 90) for m in (False, True)], "contexts")
    for geometry in data["literal_geometry"]:
        trig = constants["trig"][str(geometry["angle"])]
        mirror = geometry["mirror"]
        for point, expected in zip(constants["landmarks"], geometry["landmarks_relative_to_C"], strict=True):
            for actual, literal in zip(forward(point, trig, mirror, [205, 183]), expected, strict=True):
                near(actual, literal)
        for point, offset, key in [([31, -19], [205, 183], "pan_start_image"),
                                  ([31.25, -18.75], [205, 183], "primary_P0_image"),
                                  ([48.25, -29.75], [188, 194], "primary_P0_image"),
                                  ([67.5, 29.25], [188, 194], "primary_Q_after_pan_image")]:
            for actual, literal in zip(inverse(point, trig, mirror, offset), geometry[key], strict=True):
                near(actual, literal)
        # Source-only representative odd/even viewports, not observed GTK sizes.
        for width, height in data["static_clamp_viewports"]:
            initial = [192-width//2+13, 192-height//2-9]
            corners = [forward([x, y], trig, mirror, initial)
                       for x in (0, 256) for y in (0, 256)]
            for axis, viewport in enumerate((width, height)):
                low = math.ceil(min(p[axis] for p in corners)) + initial[axis]
                high = math.floor(max(p[axis] for p in corners)) + initial[axis]
                minimum, maximum = limits(low, high-low, viewport)
                for delta in pan["expected_offsets_relative_to_O0"]:
                    value = initial[axis] + delta[axis]
                    check(value-minimum >= 16 and maximum-value >= 16, "interior pan clamp margin")


def validate_rows(data, raw):
    reuse = data["legacy_zoom_reuse"]
    digest(raw, reuse["sha256"])
    rows = [line.split("\t") for line in raw.decode().splitlines()]
    check(len(rows) == 2400, "retained radial row count")
    check([row["row"] for row in reuse["rows"]] ==
          [1411, 1412, 1413, 1420, 1491, 1500, 2011, 2012, 2013, 2020, 2091, 2100], "row selection")
    for selected in reuse["rows"]:
        fields = rows[selected["row"]-1]
        check(fields == selected["fields"], "retained row fields")
        w, h, mirror, sx, sy, x0, y0, x, y, result = map(float, fields)
        check((sx, sy) == (1, 1.25) and mirror in (0, 1), "anisotropic retained row")
        anchor = 300*max(sx, sy) - math.hypot(x0-int(w)//2, y0-int(h)//2)
        near((math.hypot(x-int(w)//2, y-int(h)//2)+anchor)/300, result)


class TransformedDragCases(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(CASES.read_text())

    def test_immutable_source_and_fixture_provenance(self):
        data = self.data
        self.assertEqual(data["baseline_commit"], "17a1875ba049b2d349b265b6e30e41c431dda0ea")
        self.assertEqual(data["legacy_source_pin"], "afa43fae3e920210146abed514f136fd49f671b5")
        for path, expected in data["source_sha256"].items():
            with self.subTest(path=path):
                digest((ROOT/path).read_bytes(), expected)
        for path, expected in data["baseline_only_sha256"].items():
            digest(subprocess.check_output(["git", "show", data["baseline_commit"]+":"+path], cwd=ROOT), expected)
        for path, expected in data["legacy_git_blob_sha256"].items():
            digest(subprocess.check_output(["git", "show", path], cwd=ROOT), expected)
        pin = subprocess.check_output(["git", "ls-tree", "HEAD", "gimp-data"], cwd=ROOT, text=True)
        self.assertEqual(pin.split()[2], data["gimp_data_pin"])

    def test_literal_geometry_radial_clamps_and_interior_pan(self):
        validate_literals(self.data)

    def test_retained_radial_rows(self):
        validate_rows(self.data, (ROOT/self.data["legacy_zoom_reuse"]["fixture"]).read_bytes())

    def test_registration_draw_ledger_and_sanitizer_scope(self):
        data = self.data
        source = (ROOT/EVENT).read_text()
        registrations = re.findall(r'^  g_test_add_(?:data_)?func\s*\([^\n]+;', source, re.M)
        new = [line for line in registrations if '"/navigation-events/drag/' in line]
        self.assertEqual([re.search(r'"([^"]+)"', line).group(1) for line in new],
                         ["/navigation-events/drag/"+name for name in NEW_NAMES])
        self.assertEqual([line for line in registrations if line not in new], data["unchanged_event_registrations"])
        self.assertEqual(len(registrations), 31)
        start_new = source.index('/* SOURCE-ONLY 25.009/drag-zoom-scroll checkpoint.')
        old_source = source[:start_new] + source[source.index('int main(', start_new):]
        for line in new:
            old_source = old_source.replace(line + "\n", "")
        digest(old_source.encode(), data["baseline_only_sha256"][EVENT])
        start = source.index('/* Original 25.009: bounded, current-source common-angle draw checkpoint.')
        end = source.index('\n}\n', source.index('canvas_widget_anchor (void)', start))+3
        digest(source[start:end].encode(), data["unchanged_draw_block_sha256"])
        for row in data["unchanged_open_task_rows"]:
            self.assertIn(row, (ROOT/"tasks.md").read_text().splitlines())
            self.assertIn("| [ ] |", row)
        tree = ast.parse((ROOT/BUILDER).read_text())
        wanted = next(ast.literal_eval(node.value) for node in tree.body if isinstance(node, ast.Assign)
                      and any(isinstance(target, ast.Name) and target.id == "wanted" for target in node.targets))
        self.assertEqual(wanted, {"app/display/"+name+".c" for name in [
            "gimppainternavigation", "gimpmodifiersmanager", "gimpdisplayshell",
            "gimpdisplayshell-rotate", "gimpdisplayshell-tool-events", "gimpdisplayshell-scale",
            "gimpdisplayshell-scroll", "gimpdisplayshell-transform"]} | {EVENT})
        self.assertEqual(data["sanitizer_scope"]["instrumented_units"], 9)
        self.assertFalse(data["sanitizer_scope"]["leak_detection"])
        self.assertIn("NOT RUN", data["status"])
        self.assertEqual(data["sanitizer_scope"]["status"], "NOT RUN")

    def test_bounded_negative_controls(self):
        mutations = [
            lambda d: d.update(schema_version=2),
            lambda d: d["inventory"].update(main_drags=17),
            lambda d: d["constants"]["trig"]["37"].update(sin=-0.6018150231520483),
            lambda d: d["groups"][0].update(anchor=336),
            lambda d: d["groups"][1]["cases"][0]["displayed_expected"].__setitem__(1, "1.25"),
            lambda d: d["groups"][2]["expected_offsets_relative_to_O0"].__setitem__(2, [-34, 22]),
            lambda d: d["literal_geometry"][3]["primary_Q_after_pan_image"].__setitem__(0, 128),
        ]
        for index, mutate in enumerate(mutations):
            with self.subTest(mutation=index):
                changed = copy.deepcopy(self.data)
                mutate(changed)
                with self.assertRaises(ValueError):
                    validate_literals(changed)
        raw = (ROOT/self.data["legacy_zoom_reuse"]["fixture"]).read_bytes()
        with self.assertRaises(ValueError):
            validate_rows(self.data, raw+b"\n")
        changed = copy.deepcopy(self.data)
        changed["legacy_zoom_reuse"]["rows"][0]["fields"][9] = "1.25"
        with self.assertRaises(ValueError):
            validate_rows(changed, raw)
        with self.assertRaises(ValueError):
            digest((ROOT/"app/display/gimppainternavigation.c").read_bytes()+b"\n",
                   self.data["source_sha256"]["app/display/gimppainternavigation.c"])


if __name__ == "__main__":
    unittest.main()
