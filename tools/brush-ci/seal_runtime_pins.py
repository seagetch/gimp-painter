#!/usr/bin/env python3
"""Seal C from an ordinary Actions checkout; never declare UI acceptance.

Root adoption is a committed, reviewed two-key change: config.approved=true
AND pins.draft_only=false.  Committed candidate_final_commit stays null.  The
config hashes the complete gate directory except itself, plus the reviewed
source map.  Its exact committed bytes are hashed in the external receipt;
there is no self-hash, self-referential C commit, external secret or network.

Run BEFORE builds/tests create checkout files.  --source is the normal owned
PAINTER_CI_SOURCE checkout beneath RUNNER_TEMP, not GITHUB_WORKSPACE.  Both
outputs must be new files beneath RUNNER_TEMP and outside the source checkout.
The owned checkout must fetch depth 2: C has exactly one parent, accepted A,
whose actual locally fetched tree must equal the frozen accepted-tree pin.
The runner must retain the receipt and output together.  Their source proof
does not replace the common/Brush runtime, negative-control or visual gates.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess

ACCEPTED = "e25a94e21efca6e6604ddac6f15570c049f421e7"
ACCEPTED_TREE = "8634eeef49c81aaa5c25192cb76d749ff680683f"
BASELINE306 = "d4ce384f708090facbf6b08edc38b4ddcf84cff0"
REPOSITORY = "seagetch/gimp-painter"
BUNDLE_ROOT = "tools/brush-ci"
CONFIG_NAME = BUNDLE_ROOT + "/runtime-seal-config.json"
PINS_NAME = BUNDLE_ROOT + "/pins.json"
HISTORY = {
    "source_commit": "3d6f62235c6bc9e2505d2c2b6ad1d42e15d081af",
    "source_tree": "a5a14a81a10857834301888a0cd2b3cf902bf17d",
    "original_test_sha256": "49eb5a701b4f0a082c1f88f6a2f3a34546c738dfec4de1d28bf7989b12d5d664",
    "donor_commit": "1618cc5958148c7d534f9e3893681e14fcc94d80",
    "donor_tree": "253fcbdeb7c39391ddf8bad89b7877e1e18911b8",
    "donor_test_blob": "e3b3b28d1da397282dface38c04ed43d9fe1f0c7",
    "instrumented_test_sha256": "78079b8209f0bb4a06f9c44ef15a2ee393c3d917da7ecad213cee8b259c61152",
    "mode": "dockbook-controlled-order-3d6f",
    "diagnostic_only": True,
    "acceptance_eligible": False,
}
BRUSH_SOURCE = (
    "app/tests/test-painter-brush-thumbnail-popup.inc",
    "app/tests/test-painter-compact-options.c",
    "app/tools/gimppainterbrushpopup.cpp",
    "app/tools/gimppainterbrushpopup.h",
    "app/tools/gimppainterpainttoolbar.c",
    "app/tools/gimppaintertogglepopup.cpp",
    "app/tools/gimppaintertogglepopup.h",
    "app/tools/meson.build",
    "migration/inventory/additional-c-header-routes.json",
    "migration/tests/brush-thumbnail-native-popup/README.md",
    "migration/tests/brush-thumbnail-native-popup/cases.json",
    "migration/tests/brush-thumbnail-native-popup/historical-header-registration.json",
    "migration/tests/brush-thumbnail-native-popup/implementation-contract.md",
    "migration/tests/brush-thumbnail-native-popup/native-build.json",
    "migration/tests/brush-thumbnail-native-popup/retained-controller-comparison.json",
    "migration/tests/brush-thumbnail-native-popup/lower-controls-staging.json",
    "tasks.md",
    "migration/progress.tsv",
)
PROTECTED_COMMON = (
    "app/tests/test-common-layer-popup.cpp",
    "app/tests/test-painter-layer-ui.c",
    "app/tests/meson.build",
    "tools/run-painter-ui-ci.sh",
    "tools/check_painter_ui_ci.py",
    "tools/painter-common-layer-popup-cases.json",
    "tools/prepare-painter-popup-baseline.py",
    "tools/check_painter_stack_diagnostic.py",
    "tools/tests/test_painter_ui_ci.py",
    "tools/tests/test_painter_popup_baseline.py",
    "tools/prepare-linux-build-deps.py",
    "tools/linux-debian13-env.sh",
    "tools/run-painter-frozen-aggregate.py",
    "migration/baseline/debian13-package-lock.json",
    "app/tests/test-overlay-paint-gate.cpp",
    "app/widgets/gimpoverlaychild.c",
    "app/widgets/gimpoverlaychild.h",
    "tools/painter-overlay-paint-gate-cases.json",
    "tools/tests/test_painter_overlay_gate_ci.py",
    "tools/prepare-painter-dockbook-diagnostic.py",
    "tools/check_painter_dockbook_diagnostic.py",
    "tools/check_painter_controlled_order_diagnostic.py",
    "tools/painter-ci-root-capture.c",
    "tools/tests/test_painter_dockbook_diagnostic.py",
    "tools/tests/test_painter_dockbook_stack.py",
    "tools/tests/test_painter_controlled_order_stack.py",
)
REQUIRED_SOURCE = frozenset(BRUSH_SOURCE + PROTECTED_COMMON + (
    "app/widgets/gimpblobeditor.h", ".github/workflows/painter-native-ui.yml",
    "meson.build", "app/tests/test-painter-transform-popup.inc",
    "libgimp/gimp.c", "libgimp/tests/plug-in-protocol-fixture.c",
    "libgimp/tests/test-plug-in-protocol.c", "libgimp/tests/meson.build",
    "libgimpbase/gimpchoice.c", "libgimpbase/meson.build",
    "libgimpbase/test-choice-lifecycle.c",
    "libgimpbase/gimpprotocol.c", "libgimpbase/gimpwire.c",
    "app/paint/gimppaintoptions.c", "app/tests/test-painter-brush-geometry.cpp",
    "app/tests/tests.c",
    "libgimp/gimpplugin.c",
    "app/config/gimpdisplayoptions.c", "app/widgets/gimpviewrenderer.c",
    "libgimpconfig/gimpcolorconfig.c", "libgimpconfig/meson.build",
    "libgimpconfig/test-color-config-ownership.c",
    "app/tests/color-ownership/meson.build",
    "app/tests/color-ownership/test-display-renderer.c",
    "app/display/gimpcanvas.c", "app/tests/color-ownership/test-canvas.c",
    "app/tests/color-ownership/fixtures/gimpcanvas-c9.c",
    "libgimpwidgets/gimpframe.c", "libgimpwidgets/gimpframe.h",
    "libgimpwidgets/gimpwidgetstypes.h", "libgimpwidgets/gimpwidgetsutils.c",
    "libgimpwidgets/gimpwidgetsutils.h", "libgimpwidgets/meson.build",
    "libgimpwidgets/test-frame-style-idempotence.c",
    "libgimpwidgets/fixtures/gimpframe-c14.c",
    "migration/fixtures/legacy-paper-transform/pixels.tsv.gz",
    "migration/fixtures/legacy-brush-geometry/default-dynamics.tsv.gz"))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encoded(value):
    return (json.dumps(value, sort_keys=True, indent=2) + "\n").encode()


def unique_json(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("Duplicate JSON key: " + key)
            result[key] = value
        return result
    return json.loads(raw, object_pairs_hook=pairs)


def git(source, *args):
    # Do not honor inherited alternate index/worktree/object or replacement
    # controls. --no-optional-locks keeps this source inspection non-mutating.
    env = {key: value for key, value in os.environ.items()
           if not key.startswith("GIT_")}
    env["GIT_NO_REPLACE_OBJECTS"] = "1"
    return subprocess.check_output(
        ["git", "--no-optional-locks", "-c", "core.fsmonitor=false", "-c",
         "core.untrackedCache=false", "-C", str(source), *args], env=env,
        stderr=subprocess.PIPE)


def relative(name):
    if (not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_./-]+", name)
            or str(PurePosixPath(name)) != name or name.startswith("/")
            or any(part in (".", "..", ".git") for part in name.split("/"))):
        raise ValueError("Unsafe repo-relative path: " + repr(name))
    return name


def regular(source, name):
    path = source
    for part in relative(name).split("/"):
        path = path / part
        if path.is_symlink():
            raise ValueError("Symlink source path: " + name)
    if not path.is_file() or not stat.S_ISREG(path.stat().st_mode):
        raise ValueError("Missing/nonregular source: " + name)
    return path.read_bytes()


def committed(source, name):
    data = regular(source, name)
    entry = git(source, "ls-tree", "-z", "HEAD", "--", name).split(b"\0")
    if len(entry) != 2 or not entry[0]:
        raise ValueError("Missing exact tracked file: " + name)
    meta, actual_name = entry[0].split(b"\t", 1)
    mode, kind, _oid = meta.split()
    if actual_name.decode() != name or kind != b"blob" or mode not in (b"100644", b"100755"):
        raise ValueError("Nonregular tracked file: " + name)
    if data != git(source, "show", "HEAD:" + name):
        raise ValueError("Raw source differs from committed bytes: " + name)
    return data


def hash_map(value, name):
    if not isinstance(value, dict) or not value:
        raise ValueError("Missing/nonobject hash map: " + name)
    for path, digest in value.items():
        relative(path)
        if not isinstance(digest, str) or not re.fullmatch("[0-9a-f]{64}", digest):
            raise ValueError("Invalid SHA256 in " + name + ": " + path)
    return value


def clean_head(source):
    if git(source, "status", "--porcelain=v1", "--untracked-files=all",
           "--ignored=matching", "--ignore-submodules=none").strip():
        raise ValueError("Clean checkout required, including untracked/ignored files and submodules")
    for entry in git(source, "ls-files", "-v", "-z").split(b"\0"):
        if entry and not entry.startswith(b"H "):
            raise ValueError("Hidden assume-unchanged/skip-worktree index flag is forbidden")
    return git(source, "rev-parse", "HEAD").decode().strip()


def build_seal(source, config_path, environment):
    """Read-only verification; return deterministic runtime bytes + receipt."""
    if environment.get("GITHUB_ACTIONS") != "true":
        raise ValueError("BLOCKED: ordinary authorized GitHub Actions only")
    if environment.get("GITHUB_REPOSITORY") != REPOSITORY:
        raise ValueError("Wrong Actions repository")
    requested = environment.get("GITHUB_SHA", "")
    if not re.fullmatch("[0-9a-f]{40}", requested):
        raise ValueError("GITHUB_SHA must be an exact 40-character commit")
    temp = Path(environment.get("RUNNER_TEMP", "")).resolve()
    if not environment.get("RUNNER_TEMP") or not temp.is_dir():
        raise ValueError("RUNNER_TEMP must be an existing directory")
    source_arg = Path(source)
    if source_arg.is_symlink():
        raise ValueError("Symlink checkout is forbidden")
    source = source_arg.resolve(strict=True)
    if source == temp or not source.is_relative_to(temp):
        raise ValueError("Owned checkout must be below RUNNER_TEMP")
    if (not (source / ".git").is_dir() or (source / ".git").is_symlink()
            or source.stat().st_uid != os.geteuid()
            or (source / ".git").stat().st_uid != os.geteuid()):
        raise ValueError("Expected independently owned checkout and .git directory")
    if Path(git(source, "rev-parse", "--show-toplevel").decode().strip()).resolve() != source:
        raise ValueError("Source is not the Git root")
    if clean_head(source) != requested or requested == ACCEPTED:
        raise ValueError("Requested GITHUB_SHA differs from distinct clean candidate HEAD")
    parents = [line[7:].decode() for line in git(source, "cat-file", "-p", "HEAD").split(b"\n\n", 1)[0].splitlines()
               if line.startswith(b"parent ")]
    if parents != [ACCEPTED]:
        raise ValueError("Candidate must have exactly one immediate parent: accepted task307")
    if git(source, "rev-parse", ACCEPTED + "^{tree}").decode().strip() != ACCEPTED_TREE:
        raise ValueError("Actual accepted parent tree differs from the immutable pin")
    if Path(config_path).absolute() != source / CONFIG_NAME:
        raise ValueError("Config must be the expected committed gate config")
    config_raw = committed(source, CONFIG_NAME)
    config = unique_json(config_raw)
    keys = {"format", "approved", "repository", "gate_bundle_root", "source_sha256", "bundle_sha256"}
    if not isinstance(config, dict) or set(config) != keys or type(config["format"]) is not int or config["format"] != 1:
        raise ValueError("Unsupported approval config schema")
    if config["approved"] is not True:
        raise ValueError("BLOCKED: root adoption approval is not committed")
    if config["repository"] != REPOSITORY or config["gate_bundle_root"] != BUNDLE_ROOT:
        raise ValueError("Approved repository/gate root differs")
    source_map = hash_map(config["source_sha256"], "source_sha256")
    bundle_map = hash_map(config["bundle_sha256"], "bundle_sha256")
    if not REQUIRED_SOURCE <= set(source_map):
        raise ValueError("Approved source hash bundle omits required reviewed source")
    if set(source_map) & set(bundle_map) or CONFIG_NAME in source_map:
        raise ValueError("Source/bundle maps overlap or self-reference config")
    tracked = set(git(source, "ls-files", "-z", "--", BUNDLE_ROOT).decode().strip("\0").split("\0"))
    if set(bundle_map) != tracked - {CONFIG_NAME} or CONFIG_NAME not in tracked:
        raise ValueError("Complete tracked gate bundle must be hashed except config itself")
    for name in ("pins.json", "seal_runtime_pins.py", "test_runtime_pins.py"):
        if BUNDLE_ROOT + "/" + name not in bundle_map:
            raise ValueError("Required gate file omitted: " + name)
    for name, expected in sorted((source_map | bundle_map).items()):
        if sha(committed(source, name)) != expected:
            raise ValueError("Approved file hash mismatch: " + name)
    if sha(Path(__file__).read_bytes()) != bundle_map[BUNDLE_ROOT + "/seal_runtime_pins.py"]:
        raise ValueError("Executing seal generator differs from reviewed committed generator")
    pins_raw = committed(source, PINS_NAME)
    pins = unique_json(pins_raw)
    if not isinstance(pins, dict) or pins.get("draft_only") is not False:
        raise ValueError("BLOCKED: root must explicitly clear committed draft_only")
    if "candidate_final_commit" not in pins or pins["candidate_final_commit"] is not None:
        raise ValueError("Committed candidate_final_commit must remain null, never self-reference C")
    if (pins.get("accepted_task307_baseline_commit") != ACCEPTED
            or pins.get("accepted_task307_tree") != ACCEPTED_TREE
            or pins.get("integration_donor_commit") != ACCEPTED
            or pins.get("common306_baseline_commit") != BASELINE306
            or pins.get("historical_dockbook_diagnostic") != HISTORY):
        raise ValueError("Accepted baseline/tree, 306 or immutable historical identity changed")
    common = pins.get("common_protected_file_sha256")
    if not isinstance(common, dict) or set(common) != set(PROTECTED_COMMON):
        raise ValueError("Exact 26-file common protection set required")
    if any(source_map[name] != common[name] for name in PROTECTED_COMMON):
        raise ValueError("Common source hash map and immutable pins disagree")
    for name, field in (
        (BRUSH_SOURCE[0], "test_include_sha256"),
        (BRUSH_SOURCE[1], "candidate_main_sha256"),
        ("migration/tests/brush-thumbnail-native-popup/cases.json", "case_manifest_sha256"),
    ):
        if source_map[name] != pins.get(field):
            raise ValueError("Brush source hash map and pins disagree: " + field)
    sealed = copy.deepcopy(pins)
    sealed["candidate_final_commit"] = requested
    sealed["runtime_seal"] = {
        "format": 1, "status": "RUNTIME_IDENTITY_SEALED_EXECUTION_PENDING",
        "config_sha256": sha(config_raw), "committed_pins_sha256": sha(pins_raw),
        "source_map_sha256": sha(encoded(source_map)),
        "gate_bundle_map_sha256": sha(encoded(bundle_map)),
        "candidate_tree": git(source, "rev-parse", "HEAD^{tree}").decode().strip(),
        "runtime_tests_run": False, "acceptance_eligible": False,
    }
    output = encoded(sealed)
    receipt = {
        **sealed["runtime_seal"], "requested_github_sha": requested,
        "actual_clean_head": requested, "repository": REPOSITORY,
        "verified_single_immediate_parent": ACCEPTED,
        "accepted_task307_baseline_commit": ACCEPTED, "accepted_task307_tree": ACCEPTED_TREE,
        "common306_baseline_commit": BASELINE306,
        "historical_dockbook_diagnostic": copy.deepcopy(HISTORY),
        "source_file_count": len(source_map), "gate_bundle_file_count": len(bundle_map),
        "runtime_pins_sha256": sha(output),
        "scope": "Identity/source hashes only; no build, runtime, negative-control or pixel acceptance",
    }
    if clean_head(source) != requested:
        raise ValueError("Checkout changed during sealing")
    return output, receipt


def output_path(path, source, temp):
    path = Path(path).absolute()
    if path.is_symlink() or path.exists() or not path.parent.is_dir():
        raise ValueError("Output must be a new file in an existing external directory")
    resolved = path.resolve()
    if resolved.is_relative_to(source) or not resolved.is_relative_to(temp):
        raise ValueError("Output must be beneath RUNNER_TEMP and outside source")
    # Reject symlinked output parents, not merely final symlink destinations.
    for parent in path.parents:
        if parent.is_symlink():
            raise ValueError("Symlink output parent is forbidden")
    return resolved


def seal(source, config, output, receipt, environment):
    source = Path(source).absolute()
    temp = Path(environment.get("RUNNER_TEMP", "")).resolve()
    out_path = output_path(output, source.resolve(), temp)
    receipt_path = output_path(receipt, source.resolve(), temp)
    if out_path == receipt_path:
        raise ValueError("Runtime pins and receipt require distinct paths")
    data, report = build_seal(source, config, environment)
    created = []
    try:
        for path, raw in ((out_path, data), (receipt_path, encoded(report))):
            fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            created.append(path)
            with os.fdopen(fd, "wb") as stream:
                stream.write(raw)
    except BaseException:
        for path in created:
            path.unlink()
        raise
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--receipt", required=True, type=Path)
    args = parser.parse_args()
    try:
        report = seal(args.source, args.config, args.output, args.receipt, os.environ)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, "BLOCKED: runtime seal failed: " + str(error) + "\n")
    print(json.dumps({"status": report["status"], "runtime_pins_sha256": report["runtime_pins_sha256"],
                      "receipt_sha256": sha(args.receipt.read_bytes())}, sort_keys=True))


if __name__ == "__main__":
    main()
