#!/usr/bin/env python3
"""Verify/build the independent non-UI Edge raw-shadow foundation.

Source-only (no compiler, pkg-config, or native test is executed):
  python3 app/painter/tests/run_filter_edge_shadow_tests.py --verify-only
Authorized native builds must hold the shared build lane:
  flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_edge_shadow_tests.py BUILD --report NEW_REPORT
  flock /workspace/shared/gimp-painter-build.lock python3 app/painter/tests/run_filter_edge_shadow_tests.py BUILD --report NEW_REPORT --sanitize

The literal pinned legacy oracle is compiled separately as C11, production and
first/twice-header tests as C++14. Normal mode disables RTTI. Focused sanitizer
mode uses ASan/UBSan/float-cast-overflow, excludes LeakSanitizer, and makes no
leak claim. CC/CXX, CPPFLAGS/CFLAGS/CXXFLAGS/LDFLAGS are recorded and honored.
Every command requires exit zero and empty stderr; native success additionally
requires the exact complete-corpus success marker. Fresh private artifacts and
an exclusively reserved report preserve failed attempts and never replace old
evidence. Source, fixture, object, and executable hashes bracket execution.

Evidence is source-C raw arithmetic and historical final-output comparisons
(after explicit merge), never a fresh original-GIMP runtime or owner/XCF claim.
No GUI/profile/install/publication action is performed.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[3]
LEGACY_COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"
PUBLIC_BASE_COMMIT = "8cade86785af938a254216448a2a5aab3a464cd6"
PRESERVED_SOURCES = {
    "app/painter/filter-edge.cpp": "ff9d6c77932043585c8e914ee98cbfb2278eabbc8928d3ea821e6a7c955b7d67",
    "app/painter/filter-edge-kernel-private.hpp": "dbb9852b334b3279a3c9a904eef9db99efe4c1e4f8cd8beeb540ad51a65584b7",
    "app/painter/filter-raster-kernels.cpp": "70b6551e5bb4b800b3fc4145f9f07f25fe7400e52ca9543bd9bddf343bba6b06",
    "app/painter/filter-native-kernels.cpp": "960667b9b738acb5613bf4a893a7ccc16fdcbdf56e9d84100d983516b0e273d7",
    "app/painter/filter-edge.hpp": "7e7aa1053ea366f7d0991c718fcee553e2e90b41b06b553a2ef97267bc62afd2",
    "app/painter/filter-raster-kernels.hpp": "02e55a238cc5d9805376a7d623c2eac3643a4344fe18ca3597bfd61ff3cd0ee7",
    "app/painter/filter-native-kernels.hpp": "eb9d70146c661c2c63b3d7c908d4a0a656fcabd3af0f879b1f53ecb4257ebb6a",
}
MERGE_PATH = "app/paint-funcs/paint-funcs.c"
LEGACY_SOURCES = {
    MERGE_PATH: "41a5b121529718048b8cf3d6a9edd5bf67fb7e0734664393ea3c2a497d6d1cb5",
    "plug-ins/common/edge.c": "38512426b7145167e7e0b220164bd9cecee70f9b754276ad0d006adc1a18234b",
    "libgimp/gimppixelfetcher.c": "da851302005ad64b8518c90d2bb9ab17fcd3052d7ebc4c492fdbd8f511ddbcf3",
    "libgimpmath/gimpmath.h": "5be55b44288b5d3bac28e9fc51e5003af07734dc41b098f137e35ef6188f3ffe",
}
ORACLE = "app/painter/tests/legacy-edge-shadow-oracle.c"
ORACLE_HEADER = "app/painter/tests/legacy-edge-shadow-oracle.h"
ORACLE_SEALS = {
    ORACLE: "aa159258dab62dec0f3dd5d0c43d222d4e4441e8682b8712825e6cf1def63d23",
    ORACLE_HEADER: "ee5ae411e5eca8f809ea431a469956b30b12eaa30e9b8ec01f642d9b94e97c85",
}
EXPECTED_BLOCKS = {"EDGE", "EDGE DETECTORS", "PIXEL FETCHER", "CLAMP0255"}
EDGE_DIR = "migration/fixtures/legacy-edge"
GRAY_DIR = "migration/fixtures/legacy-gray-filter"
ARCHIVE_SEALS = {
    EDGE_DIR + "/capture-report.json": "c24b4804bea0b2c04e14bf544f05ec9043f533f735d74623e25181113f5b0aa3",
    EDGE_DIR + "/fixtures.tsv": "dd5e5b064c17b6d77eff4bbb42f3a94995fe3ff0fdd1107408da1913b42096d5",
    EDGE_DIR + "/cases.json": "dd01b830496b355a3ca6729eb1dbd3b2fb046aa0525221a2d051b8ed078d0faa",
    GRAY_DIR + "/capture-report.json": "27c67472ef8b9524b152b08162c786df6088e43db4ee0fb5276e6f50f2ad9dfe",
    GRAY_DIR + "/fixtures.tsv": "7efffd87e557e59634dbd54210bdf12018be25703eea9a5c595d92e6140a2669",
    GRAY_DIR + "/cases.json": "43bf1a91591fdace30edb54bb5be367081a35970463cba16da59b0df14f0013e",
    GRAY_DIR + "/inputs.json": "d64b0923c8eb43fbb240e0e93882ce9eb6b43ed9789c55607adca391b1a4724b",
}
C_SOURCES = [ORACLE]
CPP_SOURCES = [
    "app/painter/filter-raster.cpp",
    "app/painter/filter-native-kernels.cpp",
    "app/painter/filter-raster-kernels.cpp",
    "app/painter/filter-edge.cpp",
    "app/painter/filter-edge-shadow.cpp",
    "app/painter/tests/test-filter-edge-shadow.cpp",
    "app/painter/tests/test-filter-edge-shadow-header.cpp",
]
SUCCESS_MARKER = "Edge raw-shadow foundation: all checks passed (76 RGBA, 45 Gray fixtures)"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def marked(source, name):
    begin = ("/* BEGIN EXACT LEGACY " + name + " */\n").encode()
    end = ("\n/* END EXACT LEGACY " + name + " */").encode()
    if source.count(begin) != 1 or source.count(end) != 1:
        raise ValueError("Missing or ambiguous exact oracle markers: " + name)
    block = source.split(begin, 1)[1]
    if end not in block:
        raise ValueError("Reversed exact oracle markers: " + name)
    return block.split(end, 1)[0]


def verify_oracle():
    pinned = {}
    for name, expected in LEGACY_SOURCES.items():
        original = subprocess.check_output(
            ["git", "show", f"{LEGACY_COMMIT}:{name}"], cwd=ROOT,
            env=dict(os.environ, GIT_NO_LAZY_FETCH="1"))
        if hashlib.sha256(original).hexdigest() != expected:
            raise ValueError("Pinned legacy source hash differs: " + name)
        pinned[name] = original
    edge = pinned["plug-ins/common/edge.c"]
    fetch = pinned["libgimp/gimppixelfetcher.c"]
    expected = {
        "EDGE": edge[edge.index(b"static void\nedge ("):edge.index(b"\n/* ***********************   Edge Detection")].rstrip(),
        "EDGE DETECTORS": edge[edge.index(b"static gint\nedge_detect ("):edge.index(b"\n\n/*******************************************************/", edge.index(b"static gint\nedge_detect ("))].rstrip(),
        "PIXEL FETCHER": fetch[fetch.index(b"gimp_pixel_fetcher_get_pixel (GimpPixelFetcher"):fetch.index(b"\n/**\n * gimp_pixel_fetcher_put_pixel:")].rstrip(),
        "CLAMP0255": next(line for line in pinned["libgimpmath/gimpmath.h"].splitlines()
                          if line.startswith(b"#define CLAMP0255(a)")),
    }
    oracle = (ROOT / ORACLE).read_bytes()
    for name, block in expected.items():
        if marked(oracle, name) != block:
            raise ValueError("Oracle block is not the literal pinned source: " + name)
    if set(ORACLE_SEALS) != {ORACLE, ORACLE_HEADER}:
        raise ValueError("Missing exact oracle adapter/header verification seals")
    for name, expected_hash in ORACLE_SEALS.items():
        if digest(ROOT / name) != expected_hash:
            raise ValueError("Oracle types/macros/adapter/header seal differs: " + name)
    merge = pinned[MERGE_PATH]
    merge_start = merge.index(b"static inline void\nreplace_inten_pixels (")
    merge_end = merge.index(b"\n/*  replace the contents", merge_start)
    return {"source_commit": LEGACY_COMMIT, "verified_source_sha256": LEGACY_SOURCES,
            "historical_final_merge_source": {
                "path": MERGE_PATH, "sha256": LEGACY_SOURCES[MERGE_PATH],
                "replace_inten_pixels_sha256": hashlib.sha256(merge[merge_start:merge_end].rstrip()).hexdigest(),
                "note": "Tests independently specialize full coverage, all components enabled, equal unchanged source/shadow alpha; not a general selection/context merge claim"},
            "verified_blocks": {name: hashlib.sha256(block).hexdigest() for name, block in expected.items()},
            "adapter_sha256": dict(ORACLE_SEALS), "literal_blocks_match_pinned_source": True,
            "build_language": "C11, separately compiled with CC/CFLAGS",
            "defined_legacy_amount_limit": "INT32_MAX / 2040; lower amounts use the old minimum clamp",
            "note": "Source-extracted raw C oracle; no fresh original-GIMP runtime capture"}


def verify_preserved_sources():
    for name, expected in PRESERVED_SOURCES.items():
        original = subprocess.check_output(
            ["git", "show", f"{PUBLIC_BASE_COMMIT}:{name}"], cwd=ROOT,
            env=dict(os.environ, GIT_NO_LAZY_FETCH="1"))
        if hashlib.sha256(original).hexdigest() != expected:
            raise ValueError("Pinned public-base contract source hash differs: " + name)
        if (ROOT / name).read_bytes() != original:
            raise ValueError("Existing complete-output contract source changed: " + name)
    return {"public_base_commit": PUBLIC_BASE_COMMIT, "source_sha256": PRESERVED_SOURCES,
            "whole_files_byte_identical": True,
            "note": "Existing byte-vector, raster and normalized-real Edge implementation files are unchanged"}


def rows(base, gray=False):
    result = []
    for line in (base / "fixtures.tsv").read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        fields = line.split()
        if len(fields) != (9 if gray else 7):
            raise ValueError("Malformed archived fixture manifest row")
        if gray:
            proc, w, h, channels, amount, border, mode, src, dst = fields
            if proc not in ("plug-in-edge", "plug-in-gauss"):
                raise ValueError("Unknown Gray fixture procedure")
            if proc != "plug-in-edge":
                continue
        else:
            w, h, amount, border, mode, src, dst = fields
            channels = "4"
        key = (int(w), int(h), int(channels), float(amount), int(border), int(mode), src, dst)
        if (key[0] <= 0 or key[1] <= 0 or key[2] not in ((1, 2) if gray else (4,)) or
                not math.isfinite(key[3]) or key[4] not in (1, 2, 3) or key[5] not in range(6)):
            raise ValueError("Invalid archived Edge options or geometry")
        result.append(key)
    if len(result) != (45 if gray else 76) or len(set(result)) != len(result):
        raise ValueError("Incomplete or duplicate archived Edge fixture corpus")
    return result


def local_fixture(base, name):
    path = base / name
    if Path(name).name != name or path.resolve().parent != base.resolve():
        raise ValueError("Fixture path escapes archive directory: " + name)
    return path


def fixture_input_paths():
    paths = {ROOT / name for name in ARCHIVE_SEALS}
    for directory, gray in ((EDGE_DIR, False), (GRAY_DIR, True)):
        base = ROOT / directory
        paths.update(base / name for name in ("capture.scm", "capture.log", "README.md"))
        if not gray:
            paths.add(base / "capture-fixtures.tsv")
        for w, h, channels, amount, border, mode, src, dst in rows(base, gray):
            for name in (src, dst):
                paths.add(local_fixture(base, name))
                paths.add(local_fixture(base, Path(name).stem + ".png"))
            paths.add(local_fixture(base, src.replace("input-", "loaded-", 1)))
            paths.add(local_fixture(base, Path(src).stem.replace("input-", "loaded-", 1) + ".png"))
    return sorted(paths)


def verify_fixtures():
    for name, expected in ARCHIVE_SEALS.items():
        if digest(ROOT / name) != expected:
            raise ValueError("Archived provenance/manifest seal differs: " + name)
    provenance = {}
    for directory, gray in ((EDGE_DIR, False), (GRAY_DIR, True)):
        base = ROOT / directory
        capture = json.loads((base / "capture-report.json").read_text())
        total = 77 if gray else 76
        if (capture.get("source_commit") != LEGACY_COMMIT or capture.get("exit_code") != 0 or
                capture.get("case_count") != total or len(capture.get("cases", [])) != total):
            raise ValueError("Unaccepted archived capture: " + directory)
        if gray:
            if (capture.get("status") != "passed" or capture.get("errors") != [] or
                    capture.get("source_sha256", {}).get("edge.c") != LEGACY_SOURCES["plug-ins/common/edge.c"]):
                raise ValueError("Gray Edge source provenance/status differs")
        elif capture.get("all_case_markers_present") is not True:
            raise ValueError("RGBA capture has incomplete case markers")
        for name, key in (("capture.scm", "script_sha256"), ("capture.log", "log_sha256"),
                          ("fixtures.tsv" if gray else "capture-fixtures.tsv", "fixtures_tsv_sha256")):
            if digest(base / name) != capture.get(key):
                raise ValueError("Archived capture artifact seal differs: " + str(base / name))
        cases = {}
        for case in capture["cases"]:
            if gray and case["procedure"] != "plug-in-edge":
                continue
            key = (case["width"], case["height"], case["channels"] if gray else 4,
                   case["a"] if gray else case["amount"], case["b"] if gray else case["wrapmode"],
                   case["c"] if gray else case["edgemode"], Path(case["input"]).name, Path(case["output"]).name)
            if key in cases:
                raise ValueError("Duplicate archived capture case")
            cases[key] = case
        manifest = rows(base, gray)
        if set(manifest) != set(cases):
            raise ValueError("Fixture manifest and archived capture cases differ")
        for key in manifest:
            w, h, channels, amount, border, mode, src, dst = key
            case = cases[key]
            for name, expected in ((src, case["input_sha256" if gray else "input_rgba_sha256"]),
                                   (dst, case["output_sha256" if gray else "output_rgba_sha256"])):
                path = local_fixture(base, name)
                if path.stat().st_size != w * h * channels or digest(path) != expected:
                    raise ValueError("Archived original input/output bytes differ: " + str(path))
            if digest(base / (Path(dst).stem + ".png")) != case["png_sha256" if gray else "output_png_sha256"]:
                raise ValueError("Archived original output PNG differs: " + dst)
        for item in capture["inputs"]:
            if gray:
                src = item["raw"]
                expected = item["input_sha256"]
                if item["unchanged"] is not True or item["loaded_sha256"] != expected:
                    raise ValueError("Archived Gray input round-trip differs")
            else:
                src = "input-" + item["shape"] + ".rgba"
                expected = item["input_rgba_sha256"]
                if item["input_bytes_unchanged"] is not True or item["loaded_rgba_sha256"] != expected:
                    raise ValueError("Archived RGBA input round-trip differs")
                for prefix, hash_key in (("input-", "input_png_sha256"), ("loaded-", "loaded_png_sha256")):
                    if digest(base / (prefix + item["shape"] + ".png")) != item[hash_key]:
                        raise ValueError("Archived RGBA input PNG differs")
            for name in (src, src.replace("input-", "loaded-", 1)):
                if digest(local_fixture(base, name)) != expected:
                    raise ValueError("Archived loaded source bytes differ: " + name)
        provenance["gray_edge" if gray else "rgba_edge"] = {
            "case_count": len(manifest), "capture_report": directory + "/capture-report.json",
            "captured_utc": capture["captured_utc"], "source_commit": LEGACY_COMMIT,
            "archived_executable_sha256": capture["executables"],
            "note": "Historical full-drawable final PDB outputs; compare after explicit full-coverage merge"}
    return fixture_input_paths(), provenance


def source_paths():
    pending = [ROOT / name for name in C_SOURCES + CPP_SOURCES]
    seen = set()
    while pending:
        path = pending.pop().resolve()
        path.relative_to(ROOT.resolve())
        if path in seen:
            continue
        seen.add(path)
        for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', path.read_text(), re.MULTILINE):
            for candidate in (path.parent / name, ROOT / "app/painter" / name):
                if candidate.is_file():
                    pending.append(candidate)
                    break
            else:
                raise ValueError(f"Unresolved local include {name} in {path}")
    seen.update((Path(__file__).resolve(), ROOT / "app/painter/meson.build",
                 ROOT / "migration/contracts/edge-raw-shadow-foundation.md",
                 ROOT / "app/painter/tests/test_filter_edge_shadow_runner.py"))
    return sorted(seen)


def hash_paths(paths):
    return {str(path.relative_to(ROOT)): digest(path) for path in paths}


def snapshot_paths(before):
    return {name: digest(ROOT / name) if (ROOT / name).is_file() else None for name in before}


def changed_paths(before, after=None):
    after = snapshot_paths(before) if after is None else after
    return [name for name, sha in before.items() if after[name] != sha]


def artifact_hashes(paths):
    return {Path(path).name: digest(Path(path)) if Path(path).is_file() else None for path in paths}


def flags(name):
    return shlex.split(os.environ.get(name, ""))


def append_options(env, name, required):
    previous = env.get(name, "")
    env[name] = previous + (":" if previous else "") + ":".join(required)


def write_report(path, report):
    path.write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, nargs="?")
    parser.add_argument("--report", type=Path)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if not args.verify_only and (args.build is None or args.report is None):
        parser.error("BUILD and --report are required unless --verify-only is selected")
    report = {
        "scope": "Source/oracle/archived-fixture verification only" if args.verify_only else
                 "Non-UI Edge raw-shadow source-C oracle, vector/spill checks and historical final-output regressions",
        "verification_mode": "source-only" if args.verify_only else "native",
        "requested_sanitize": args.sanitize,
        "source_only_verified": False, "exact_oracle_verified": False, "contract_preservation_verified": False,
        "sanitizers": [] if args.verify_only or not args.sanitize else ["address", "undefined", "float-cast-overflow"],
        "leak_detection": False if args.sanitize and not args.verify_only else None,
        "leak_detection_note": "LeakSanitizer excluded; no leak claim",
        "not_covered": ["Fresh original-GIMP runtime capture", "Native FilterLayer/XCF integration", "Owner merge disposition",
                        "Scheduler/owner lifetime", "UI", "Windows/macOS execution", "Actual I/O beyond 4 GiB", "LeakSanitizer"],
        "commands": [], "all_passed": False, "stage": "verification",
    }
    if args.report:
        # Exclusive creation happens before any verification or native command.
        with args.report.open("x") as destination:
            destination.write(json.dumps(report, indent=2) + "\n")
        args.report.chmod(0o600)
    started = time.monotonic()
    try:
        sources = hash_paths(source_paths())
        fixture_paths = fixture_input_paths()
        fixtures = hash_paths(fixture_paths)
        before = {**sources, **fixtures}
        report.update(source_sha256=sources, fixture_sha256=fixtures)
        preserved = verify_preserved_sources()
        if (preserved.get("whole_files_byte_identical") is not True or
                preserved.get("source_sha256") != PRESERVED_SOURCES):
            raise ValueError("Missing exact existing-contract preservation evidence")
        report.update(preserved_complete_output_contract=preserved, contract_preservation_verified=True)
        oracle = verify_oracle()
        if (oracle.get("literal_blocks_match_pinned_source") is not True or
                set(oracle.get("verified_blocks", {})) != EXPECTED_BLOCKS or
                oracle.get("adapter_sha256") != ORACLE_SEALS or not ORACLE_SEALS):
            raise ValueError("Missing exact oracle verification evidence")
        verified_paths, provenance = verify_fixtures()
        if verified_paths != fixture_paths:
            raise ValueError("Fixture input set changed during verification")
        report["input_sha256_after_verify"] = snapshot_paths(before)
        report["changed_after_verify"] = changed_paths(before, report["input_sha256_after_verify"])
        if report["changed_after_verify"]:
            raise ValueError("Inputs changed during seal verification")
        report.update(legacy_oracle=oracle, legacy_fixtures=provenance, exact_oracle_verified=True)
        if args.verify_only:
            report.update(all_passed=None, source_only_verified=True, stage="source-only-complete")
            return 0

        args.build.mkdir(parents=True, exist_ok=True)
        out = Path(tempfile.mkdtemp(prefix="filter-edge-shadow-" + ("sanitize-" if args.sanitize else "normal-"),
                                    dir=args.build.resolve()))
        temporary = out / "tmp"
        temporary.mkdir(mode=0o700)
        env = dict(os.environ, TMPDIR=str(temporary))
        sanitize = (["-fsanitize=address,undefined,float-cast-overflow", "-fno-sanitize=leak",
                     "-fno-omit-frame-pointer"] if args.sanitize else [])
        if args.sanitize:
            append_options(env, "ASAN_OPTIONS", ["detect_leaks=0", "halt_on_error=1", "abort_on_error=1"])
            append_options(env, "UBSAN_OPTIONS", ["halt_on_error=1", "print_stacktrace=1"])
        report["output_directory"] = str(out)
        report["build_environment"] = {name: env.get(name, "") for name in
                                       ("CC", "CXX", "CPPFLAGS", "CFLAGS", "CXXFLAGS", "LDFLAGS", "ASAN_OPTIONS", "UBSAN_OPTIONS")}

        def invoke(command, kind):
            report["stage"] = kind
            record = {"kind": kind, "command": command}
            report["commands"].append(record)
            try:
                run = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, text=True, timeout=180)
                record.update(exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr)
                record["classification"] = ("nonzero-exit" if run.returncode != 0 else
                                            "zero-exit-with-stderr" if run.stderr else "clean-zero")
                print(run.stdout, end="")
                if run.stderr:
                    print(run.stderr, end="", file=sys.stderr)
                return record["classification"] == "clean-zero"
            except subprocess.TimeoutExpired as error:
                def as_text(value):
                    return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
                record.update(exit_code=None, classification="timeout", stdout=as_text(error.stdout), stderr=as_text(error.stderr))
            except OSError as error:
                record.update(exit_code=None, classification="launch-error", error=str(error))
            return False

        cc, cxx = flags("CC") or ["cc"], flags("CXX") or ["c++"]
        for command, kind in (([*cc, "--version"], "c-compiler-version"),
                              ([*cxx, "--version"], "c++-compiler-version"),
                              (["pkg-config", "--cflags", "glib-2.0"], "pkg-cflags"),
                              (["pkg-config", "--libs", "glib-2.0"], "pkg-libs")):
            if not invoke(command, kind):
                return 1
        pkg = shlex.split(report["commands"][-2]["stdout"])
        libs = shlex.split(report["commands"][-1]["stdout"])
        common = ["-Wall", "-Wextra", "-Werror", "-g", "-O1" if args.sanitize else "-O2", "-pthread"]
        objects = []
        object_seals = {}
        report["object_sha256"] = object_seals
        for source in C_SOURCES + CPP_SOURCES:
            obj = out / (Path(source).name + ".o")
            is_c = source in C_SOURCES
            language = ([*flags("CFLAGS"), "-std=c11", "-x", "c"] if is_c else
                        [*flags("CXXFLAGS"), "-std=c++14", "-fexceptions", "-frtti" if args.sanitize else "-fno-rtti", *pkg])
            command = [*(cc if is_c else cxx), *common, *flags("CPPFLAGS"), *language, "-UNDEBUG", *sanitize,
                       "-I" + str(ROOT / "app/painter"), "-c", str(ROOT / source), "-o", str(obj)]
            if not invoke(command, "compile-c" if is_c else "compile-c++"):
                return 1
            objects.append(str(obj))
            object_seals[obj.name] = digest(obj)
        report["object_sha256"] = object_seals
        report["object_sha256_before_link"] = artifact_hashes(objects)
        report["input_sha256_after_compile"] = snapshot_paths(before)
        report["changed_after_compile"] = changed_paths(before, report["input_sha256_after_compile"])
        if report["changed_after_compile"] or report["object_sha256_before_link"] != object_seals:
            report["stage"] = "compile-seal-failed"
            return 1
        executable = out / "painter-filter-edge-shadow"
        if not invoke([*cxx, *objects, *flags("LDFLAGS"), *sanitize, "-pthread", *libs, "-lm", "-o", str(executable)], "link"):
            return 1
        report["executable_sha256"] = digest(executable)
        report["object_sha256_before_run"] = artifact_hashes(objects)
        report["input_sha256_before_run"] = snapshot_paths(before)
        report["changed_before_run"] = changed_paths(before, report["input_sha256_before_run"])
        if report["changed_before_run"] or report["object_sha256_before_run"] != object_seals:
            report["stage"] = "pre-run-seal-failed"
            return 1
        success = invoke([str(executable), str(ROOT / EDGE_DIR), str(ROOT / GRAY_DIR)], "test")
        record = report["commands"][-1]
        report["exact_success_marker_verified"] = record.get("stdout", "").splitlines().count(SUCCESS_MARKER) == 1
        report["executable_sha256_after_run"] = digest(executable) if executable.is_file() else None
        report["object_sha256_after_run"] = artifact_hashes(objects)
        report["input_sha256_after_run"] = snapshot_paths(before)
        report["changed_after_run"] = changed_paths(before, report["input_sha256_after_run"])
        report["objects_unchanged"] = object_seals == report["object_sha256_after_run"]
        report["executable_unchanged"] = report["executable_sha256"] == report["executable_sha256_after_run"]
        report["all_passed"] = (success and report["exact_success_marker_verified"] and
                                report["objects_unchanged"] and report["executable_unchanged"] and
                                not report["changed_after_run"])
        report["stage"] = "complete" if report["all_passed"] else "test-or-post-run-seal-failed"
        return 0 if report["all_passed"] else 1
    except (OSError, subprocess.SubprocessError, ValueError, KeyError) as error:
        report["error"] = str(error)
        return 1
    finally:
        report["elapsed_seconds"] = time.monotonic() - started
        if args.report:
            write_report(args.report, report)
        if args.verify_only:
            print(json.dumps(report, indent=2))


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"Edge-shadow verification/build setup failed: {error}", file=sys.stderr)
        raise SystemExit(1)
