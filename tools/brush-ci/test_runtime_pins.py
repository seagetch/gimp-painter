#!/usr/bin/env python3
"""Runtime-seal unit tests using small synthetic temporary Git repositories.

No shared source Git state, real CI, GUI, network or acceptance execution.
Only module constants A/tree are substituted to identify synthetic fixture A;
there is no CLI/environment override for actual immutable identity constants.
"""
from __future__ import annotations
import copy
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

import seal_runtime_pins as gate

HERE = Path(__file__).resolve().parent


class RuntimePins(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="brush-seal-unit-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "owned-checkout"
        self.source.mkdir()
        self.g("init", "--quiet")
        self.g("config", "user.name", "Synthetic fixture")
        self.g("config", "user.email", "fixture@example.invalid")
        self.write("fixture-base.txt", b"synthetic accepted parent\n")
        self.g("add", ".")
        self.g("commit", "--quiet", "-m", "Synthetic accepted A")
        self.accepted = self.g("rev-parse", "HEAD").decode().strip()
        self.accepted_tree = self.g("rev-parse", "HEAD^{tree}").decode().strip()
        self.constants = mock.patch.multiple(gate, ACCEPTED=self.accepted, ACCEPTED_TREE=self.accepted_tree)
        self.constants.start()
        self.addCleanup(self.constants.stop)
        for name in gate.REQUIRED_SOURCE:
            self.write(name, ("synthetic file " + name + "\n").encode())
        self.write(gate.BUNDLE_ROOT + "/seal_runtime_pins.py", (HERE / "seal_runtime_pins.py").read_bytes())
        self.write(gate.BUNDLE_ROOT + "/test_runtime_pins.py", (HERE / "test_runtime_pins.py").read_bytes())
        self.write(gate.BUNDLE_ROOT + "/fixture-helper.py", b"# synthetic helper\n")
        self.source_map = {name: gate.sha((self.source / name).read_bytes()) for name in gate.REQUIRED_SOURCE}
        self.pins = {
            "draft_only": False, "candidate_final_commit": None,
            "accepted_task307_baseline_commit": self.accepted,
            "accepted_task307_tree": self.accepted_tree,
            "integration_donor_commit": self.accepted,
            "common306_baseline_commit": gate.BASELINE306,
            "historical_dockbook_diagnostic": copy.deepcopy(gate.HISTORY),
            "common_protected_file_sha256": {name: self.source_map[name] for name in gate.PROTECTED_COMMON},
            "test_include_sha256": self.source_map[gate.BRUSH_SOURCE[0]],
            "candidate_main_sha256": self.source_map[gate.BRUSH_SOURCE[1]],
            "case_manifest_sha256": self.source_map["migration/tests/brush-thumbnail-native-popup/cases.json"],
        }
        self.config = {"format": 1, "approved": True, "repository": gate.REPOSITORY,
                       "gate_bundle_root": gate.BUNDLE_ROOT, "source_sha256": self.source_map,
                       "bundle_sha256": {}}
        self.save_config()
        self.g("add", ".")
        self.g("commit", "--quiet", "-m", "Synthetic reviewed candidate C")
        self.env = {"GITHUB_ACTIONS": "true", "GITHUB_REPOSITORY": gate.REPOSITORY,
                    "GITHUB_SHA": self.g("rev-parse", "HEAD").decode().strip(),
                    "RUNNER_TEMP": str(self.root), "GITHUB_WORKSPACE": str(self.root / "different-workspace")}
        self.config_path = self.source / gate.CONFIG_NAME
        self.out = self.root / "runtime-pins.json"
        self.receipt = self.root / "runtime-seal-receipt.json"

    def g(self, *args):
        return subprocess.check_output(["git", "-C", str(self.source), *args], stderr=subprocess.PIPE,
                                       env={**os.environ, "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": "/dev/null"})

    def write(self, name, raw):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)

    def save_config(self):
        self.write(gate.PINS_NAME, gate.encoded(self.pins))
        self.config["bundle_sha256"] = {
            path.relative_to(self.source).as_posix(): gate.sha(path.read_bytes())
            for path in (self.source / gate.BUNDLE_ROOT).rglob("*")
            if path.is_file() and path.relative_to(self.source).as_posix() != gate.CONFIG_NAME
        }
        self.write(gate.CONFIG_NAME, gate.encoded(self.config))

    def amend(self, save=True):
        if save:
            self.save_config()
        self.g("add", "-A")
        self.g("commit", "--quiet", "--amend", "--no-edit")
        self.env["GITHUB_SHA"] = self.g("rev-parse", "HEAD").decode().strip()

    def build(self):
        return gate.build_seal(self.source, self.config_path, self.env)

    def fails(self, message):
        with self.assertRaisesRegex(ValueError, message):
            self.build()
        self.assertFalse(self.out.exists())
        self.assertFalse(self.receipt.exists())

    def test_exact_clean_owned_candidate_seals_only_external_output(self):
        before = self.g("status", "--porcelain=v1")
        original = (self.source / gate.PINS_NAME).read_bytes()
        result = gate.seal(self.source, self.config_path, self.out, self.receipt, self.env)
        sealed = json.loads(self.out.read_bytes())
        self.assertEqual(sealed["candidate_final_commit"], self.env["GITHUB_SHA"])
        self.assertIsNone(json.loads(original)["candidate_final_commit"])
        self.assertEqual((self.source / gate.PINS_NAME).read_bytes(), original)
        self.assertEqual(self.g("status", "--porcelain=v1"), before)
        self.assertEqual(result["runtime_pins_sha256"], gate.sha(self.out.read_bytes()))
        self.assertEqual(result, json.loads(self.receipt.read_bytes()))
        self.assertFalse(result["acceptance_eligible"])
        self.assertFalse(result["runtime_tests_run"])
        self.assertEqual(result["verified_single_immediate_parent"], self.accepted)
        self.assertEqual(self.out.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.build()[0], self.out.read_bytes())

    def test_source_is_not_required_to_equal_github_workspace(self):
        self.env["GITHUB_WORKSPACE"] = "/not-the-owned-checkout"
        self.assertEqual(self.build()[1]["actual_clean_head"], self.env["GITHUB_SHA"])

    def test_ordinary_actions_required(self):
        self.env["GITHUB_ACTIONS"] = "false"
        self.fails("ordinary authorized")

    def test_repository_must_match(self):
        self.env["GITHUB_REPOSITORY"] = "different/repository"
        self.fails("repository")

    def test_exact_github_sha_required(self):
        for wrong in ("", "HEAD", "0" * 39, "A" * 40, "0" * 40):
            self.env["GITHUB_SHA"] = wrong
            self.fails("GITHUB_SHA")

    def test_source_must_be_under_runner_temp(self):
        other = self.root / "other"
        other.mkdir()
        self.env["RUNNER_TEMP"] = str(other)
        self.fails("below RUNNER_TEMP")

    def test_source_must_be_owned(self):
        with mock.patch.object(gate.os, "geteuid", return_value=os.geteuid() + 1):
            self.fails("owned checkout")

    def test_parent_must_be_exact_immediate_accepted_commit(self):
        self.write("later.txt", b"later\n")
        self.g("add", ".")
        self.g("commit", "--quiet", "-m", "Unreviewed second candidate")
        self.env["GITHUB_SHA"] = self.g("rev-parse", "HEAD").decode().strip()
        self.fails("exactly one immediate parent")

    def test_actual_accepted_tree_is_verified(self):
        with mock.patch.object(gate, "ACCEPTED_TREE", "0" * 40):
            self.fails("Actual accepted parent tree")

    def test_wrong_config_path_rejected(self):
        self.config_path = self.root / "external-config.json"
        self.fails("expected committed gate config")

    def test_both_adoption_keys_are_required(self):
        self.config["approved"] = False
        self.amend()
        self.fails("root adoption approval")
        self.config["approved"] = True
        self.pins["draft_only"] = True
        self.amend()
        self.fails("clear committed draft_only")

    def test_committed_candidate_pin_must_stay_null(self):
        self.pins["candidate_final_commit"] = "a" * 40
        self.amend()
        self.fails("must remain null")

    def test_baseline_and_history_cannot_change(self):
        for key, value in (("accepted_task307_baseline_commit", "0" * 40),
                           ("accepted_task307_tree", "0" * 40),
                           ("integration_donor_commit", "0" * 40),
                           ("common306_baseline_commit", "0" * 40),
                           ("historical_dockbook_diagnostic", {})):
            original = self.pins[key]
            self.pins[key] = value
            self.amend()
            self.fails("immutable historical identity")
            self.pins[key] = original

    def test_exact_config_schema_and_types(self):
        self.config["format"] = True
        self.amend()
        self.fails("config schema")
        self.config["format"] = 1
        self.config["allow_dirty"] = True
        self.amend()
        self.fails("config schema")

    def test_config_repository_and_root_are_fixed(self):
        self.config["gate_bundle_root"] = "tools/other"
        self.amend()
        self.fails("repository/gate root")

    def test_missing_source_pin_rejected(self):
        self.source_map.pop("tasks.md")
        self.amend()
        self.fails("omits required")

    def test_frame_production_fixture_headers_and_build_are_required_pins(self):
        paths = ['libgimpwidgets/gimpframe.c', 'libgimpwidgets/gimpframe.h',
                 'libgimpwidgets/gimpwidgetstypes.h', 'libgimpwidgets/gimpwidgetsutils.c',
                 'libgimpwidgets/gimpwidgetsutils.h', 'libgimpwidgets/meson.build',
                 'libgimpwidgets/test-frame-style-idempotence.c',
                 'libgimpwidgets/fixtures/gimpframe-c14.c']
        self.assertTrue(set(paths).issubset(gate.REQUIRED_SOURCE))
        self.source_map.pop(paths[-1])
        self.amend()
        self.fails('omits required')

    def test_root_meson_build_is_a_required_source_pin(self):
        self.assertIn("meson.build", gate.REQUIRED_SOURCE)
        self.source_map.pop("meson.build")
        self.amend()
        self.fails("omits required")

    def test_transform_helper_is_a_required_source_pin(self):
        path = "app/tests/test-painter-transform-popup.inc"
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_libgimp_cleanup_is_a_required_source_pin(self):
        path = 'libgimp/gimp.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_protocol_fixture_is_a_required_source_pin(self):
        path = 'libgimp/tests/plug-in-protocol-fixture.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_protocol_host_is_a_required_source_pin(self):
        path = 'libgimp/tests/test-plug-in-protocol.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_protocol_registration_is_a_required_source_pin(self):
        path = 'libgimp/tests/meson.build'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_choice_production_is_a_required_source_pin(self):
        path = 'libgimpbase/gimpchoice.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_choice_registration_is_a_required_source_pin(self):
        path = 'libgimpbase/meson.build'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_choice_test_is_a_required_source_pin(self):
        path = 'libgimpbase/test-choice-lifecycle.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_wire_protocol_production_is_a_required_source_pin(self):
        path = 'libgimpbase/gimpprotocol.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_wire_dispatch_production_is_a_required_source_pin(self):
        path = 'libgimpbase/gimpwire.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_native_paint_model_is_a_required_source_pin(self):
        path = 'app/paint/gimppaintoptions.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_geometry_regression_is_a_required_source_pin(self):
        path = 'app/tests/test-painter-brush-geometry.cpp'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_plugin_cycle_provider_is_a_required_source_pin(self):
        path = 'libgimp/gimpplugin.c'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_geometry_mask_oracle_is_a_required_source_pin(self):
        path = 'migration/fixtures/legacy-paper-transform/pixels.tsv.gz'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_geometry_dynamics_oracle_is_a_required_source_pin(self):
        path = 'migration/fixtures/legacy-brush-geometry/default-dynamics.tsv.gz'
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_core_headless_initializer_is_a_required_source_pin(self):
        path = "app/tests/tests.c"
        self.assertIn(path, gate.REQUIRED_SOURCE)
        self.source_map.pop(path)
        self.amend()
        self.fails("omits required")

    def test_changed_source_hash_rejected(self):
        self.write("tasks.md", b"unreviewed changed task file\n")
        self.amend()
        self.fails("Approved file hash mismatch")

    def test_common_map_must_agree_with_pins(self):
        self.pins["common_protected_file_sha256"][gate.PROTECTED_COMMON[0]] = "0" * 64
        self.amend()
        self.fails("Common source hash map")

    def test_common_set_cannot_be_reduced(self):
        self.pins["common_protected_file_sha256"].pop(gate.PROTECTED_COMMON[0])
        self.amend()
        self.fails("Exact 26-file")

    def test_brush_map_must_agree_with_pins(self):
        self.pins["test_include_sha256"] = "0" * 64
        self.amend()
        self.fails("Brush source hash map")

    def test_tracked_gate_omission_rejected(self):
        self.config["bundle_sha256"].pop(gate.BUNDLE_ROOT + "/fixture-helper.py")
        self.write(gate.CONFIG_NAME, gate.encoded(self.config))
        self.amend(save=False)
        self.fails("Complete tracked gate bundle")

    def test_generator_itself_must_match_reviewed_bytes(self):
        self.write(gate.BUNDLE_ROOT + "/seal_runtime_pins.py", b"# replaced generator\n")
        self.amend()
        self.fails("Executing seal generator")

    def test_config_cannot_self_hash(self):
        self.source_map[gate.CONFIG_NAME] = "0" * 64
        self.amend()
        self.fails("self-reference config")

    def test_invalid_hash_rejected(self):
        self.source_map["tasks.md"] = "invalid"
        self.amend()
        self.fails("Invalid SHA256")

    def test_unsafe_relative_paths_rejected(self):
        for unsafe in ("../escape", "/absolute", "app//duplicate", "app/./dot", ".git/config", "app/../outside"):
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                gate.relative(unsafe)

    def test_duplicate_json_keys_rejected(self):
        raw = (self.source / gate.CONFIG_NAME).read_bytes()
        self.write(gate.CONFIG_NAME, raw.replace(b'"approved": true', b'"approved": true, "approved": true'))
        self.amend(save=False)
        self.fails("Duplicate JSON key")

    def test_dirty_staged_untracked_and_ignored_files_fail(self):
        path = self.source / "tasks.md"
        original = path.read_bytes()
        path.write_bytes(original + b"dirty\n")
        self.fails("Clean checkout")
        self.g("add", "tasks.md")
        self.fails("Clean checkout")
        self.g("reset", "--quiet", "HEAD", "--", "tasks.md")
        path.write_bytes(original)
        self.write("untracked.txt", b"untracked\n")
        self.fails("Clean checkout")
        (self.source / "untracked.txt").unlink()
        self.write(".gitignore", b"ignored.txt\n")
        self.amend()
        self.write("ignored.txt", b"ignored\n")
        self.fails("Clean checkout")

    def test_hidden_index_flags_fail_even_without_dirty_files(self):
        self.g("update-index", "--assume-unchanged", "tasks.md")
        self.fails("Hidden assume-unchanged/skip-worktree")

    def test_tracked_symlink_source_rejected(self):
        path = self.source / "tasks.md"
        path.unlink()
        path.symlink_to("fixture-base.txt")
        self.amend()
        self.fails("Symlink source")

    def test_source_symlink_rejected(self):
        link = self.root / "source-link"
        link.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "Symlink checkout"):
            gate.build_seal(link, self.config_path, self.env)

    def test_inside_source_output_and_overwrite_rejected(self):
        with self.assertRaisesRegex(ValueError, "outside source"):
            gate.seal(self.source, self.config_path, self.source / "runtime.json", self.receipt, self.env)
        self.out.write_bytes(b"do not replace\n")
        with self.assertRaisesRegex(ValueError, "new file"):
            gate.seal(self.source, self.config_path, self.out, self.receipt, self.env)
        self.assertEqual(self.out.read_bytes(), b"do not replace\n")

    def test_output_paths_must_be_distinct(self):
        with self.assertRaisesRegex(ValueError, "distinct"):
            gate.seal(self.source, self.config_path, self.out, self.out, self.env)

    def test_symlink_output_parent_rejected(self):
        real = self.root / "real-out"
        real.mkdir()
        link = self.root / "linked-out"
        link.symlink_to(real, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "Symlink output parent"):
            gate.seal(self.source, self.config_path, link / "pins.json", self.receipt, self.env)

    def test_seal_failure_leaves_no_output_files(self):
        self.config["approved"] = False
        self.amend()
        with self.assertRaisesRegex(ValueError, "adoption approval"):
            gate.seal(self.source, self.config_path, self.out, self.receipt, self.env)
        self.assertFalse(self.out.exists())
        self.assertFalse(self.receipt.exists())

    def test_source_bundle_overlap_rejected_even_with_matching_hash(self):
        name = gate.BUNDLE_ROOT + "/fixture-helper.py"
        self.source_map[name] = gate.sha((self.source / name).read_bytes())
        self.amend()
        self.fails("Source/bundle maps overlap or self-reference config")

    def test_source_map_config_self_reference_rejected(self):
        self.source_map[gate.CONFIG_NAME] = "0" * 64
        self.amend()
        self.fails("Source/bundle maps overlap or self-reference config")


class ActualCommittedMapPartition(unittest.TestCase):
    def test_actual_candidate_maps_are_disjoint_and_bundle_complete(self):
        config = gate.unique_json((HERE / "runtime-seal-config.json").read_bytes())
        source = gate.hash_map(config["source_sha256"], "source_sha256")
        bundle = gate.hash_map(config["bundle_sha256"], "bundle_sha256")
        self.assertFalse(set(source) & set(bundle))
        self.assertNotIn(gate.CONFIG_NAME, source)
        self.assertTrue(gate.REQUIRED_SOURCE <= set(source))
        actual = {p.relative_to(HERE.parent.parent).as_posix()
                  for p in HERE.rglob("*") if p.is_file() and
                  p.suffix != ".pyc" and p.name != "runtime-seal-config.json"}
        self.assertEqual(set(bundle), actual)
        for name in ("test-matched-preclick.c", "matched-preclick-phases.inc",
                     "meson.append", "PLAN.md", "manifest.json", "policy.json"):
            path = gate.BUNDLE_ROOT + "/matched-preclick/" + name
            self.assertIn(path, bundle)
            self.assertNotIn(path, source)
            self.assertEqual(bundle[path], gate.sha((HERE / "matched-preclick" / name).read_bytes()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
