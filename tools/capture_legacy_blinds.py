#!/usr/bin/env python3
"""Capture Blinds bytes from the actual pinned old GIMP PDB.

Source the legacy build env.sh first. Preparation does not run GIMP; --capture
holds the shared build/test lock and creates a new disposable profile/registry.
No replacement kernel generates expected pixels. Existing capture reports are
sealed; use another --output directory for an independent repeat.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import selectors
import signal
import subprocess
import tempfile
import time

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"
GEOMETRIES = ((1, 1), (2, 3), (9, 8), (67, 66), (41, 53))
PARAMETERS = ((0, 1), (15, 3), (67, 7), (90, 100))
TRANSPARENCY = (0, 1, -1, 2)
BACKGROUND = (31, 121, 217)
COLUMNS = ("procedure", "width", "height", "channels", "angle", "segments",
           "orientation", "transparent", "background_r", "background_g",
           "background_b", "input", "output")
SOURCES = ("plug-ins/common/blinds.c", "plug-ins/common/file-png.c",
           "libgimp/gimpdrawable.c", "libgimpcolor/gimprgb.c", "libgimpcolor/gimprgb.h",
           "app/base/pixel-region.c", "app/paint-funcs/paint-funcs.c")


def sha(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def quote(value: object) -> str:
    return json.dumps(str(value), ensure_ascii=False)


def gray_background(source: Path) -> tuple[int, dict[str, float]]:
    """Read the pinned old coefficients, not modern Babl luminance conversion."""
    header = (source / "libgimpcolor/gimprgb.h").read_text()
    coefficients = {}
    for name in ("RED", "GREEN", "BLUE"):
        match = re.search(r"#define\s+GIMP_RGB_LUMINANCE_" + name + r"\s+\(([\d.]+)\)", header)
        if not match:
            raise RuntimeError("Cannot find old luminance coefficient " + name)
        coefficients[name.lower()] = float(match.group(1))
    implementation = (source / "libgimpcolor/gimprgb.c").read_text()
    if "return ROUND (gimp_rgb_luminance (rgb) * 255.0);" not in implementation:
        raise RuntimeError("Pinned old luminance byte rounding changed")
    value = math.floor(sum(c * v for c, v in zip(coefficients.values(), BACKGROUND)) + 0.5)
    return value, coefficients


def prepare(out: Path, gray: int) -> tuple[list[dict], list[dict]]:
    inputs, cases, script = [], [], ["; Actual pinned legacy Blinds PDB byte capture."]

    def load(filename: str) -> str:
        path = quote(out / filename)
        return (f"(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE {path} {path}))) "
                "(layer (car (gimp-image-get-active-layer image))))")

    def save(filename: str) -> str:
        path = quote(out / filename)
        return f"(file-png-save2 RUN-NONINTERACTIVE image layer {path} {path} 0 9 0 0 0 0 0 0 1)"

    for mode in ("RGBA", "LA"):
        channels = 4 if mode == "RGBA" else 2
        for width, height in GEOMETRIES:
            label = f"{mode}-{width}x{height}"
            data = bytearray()
            for y in range(height):
                for x in range(width):
                    rgb = ((x * 37 + y * 61 + 11) % 256,
                           (x * 97 + y * 43 + 61) % 256,
                           (x * 13 + y * 173 + 137) % 256)
                    data.extend((rgb if mode == "RGBA" else rgb[:1]) + ((0, 1, 127, 255)[(x + 3 * y) % 4],))
            generated = "input-" + label + ".raw"
            loaded = "loaded-" + label + ".raw"
            png = "input-" + label + ".png"
            (out / generated).write_bytes(data)
            Image.frombytes(mode, (width, height), bytes(data)).save(out / png, compress_level=9)
            inputs.append(dict(id=label, mode=mode, width=width, height=height, channels=channels,
                               generated=generated, input_png=png, loaded=loaded,
                               loaded_png="loaded-" + label + ".png", generated_sha256=sha(out / generated)))
            script.append(load(png) + "\n " + save("loaded-" + label + ".png") +
                          f'\n (gimp-message "BLINDS_INPUT_LOADED={label}") (gimp-image-delete image))')
            for orientation in (0, 1):
                for angle, segments in PARAMETERS:
                    for transparent in TRANSPARENCY:
                        ident = f"{label}-a{angle}-s{segments}-o{orientation}-t{transparent}"
                        bg = BACKGROUND if mode == "RGBA" else (gray,) * 3
                        case = dict(id=ident, procedure="plug-in-blinds", mode=mode,
                                    width=width, height=height, channels=channels, angle=angle,
                                    segments=segments, orientation=orientation, transparent=transparent,
                                    background_r=bg[0], background_g=bg[1], background_b=bg[2],
                                    input=loaded, output=ident + ".raw", output_png=ident + ".png")
                        cases.append(case)
                        script.append(load(png) + "\n (gimp-context-set-background '(31 121 217))" +
                                      f'\n (gimp-message "BLINDS_CASE_START={ident}")' +
                                      f"\n (plug-in-blinds RUN-NONINTERACTIVE image layer {angle} {segments} {orientation} {transparent})" +
                                      f'\n (gimp-message "BLINDS_PDB_DONE={ident}")\n ' + save(case["output_png"]) +
                                      f'\n (gimp-message "BLINDS_CASE_DONE={ident}") (gimp-image-delete image))')
    assert len(cases) == 320
    (out / "capture.scm").write_text("\n\n".join(script) + "\n")
    (out / "fixtures.tsv").write_text("# " + " ".join(COLUMNS) + "\n" + "".join(
        "\t".join(str(case[key]) for key in COLUMNS) + "\n" for case in cases))
    (out / ".gitattributes").write_text("*.raw binary\n")
    return inputs, cases


def run_capture(command: list[str], env: dict[str, str], logfile: Path,
                timeout: float) -> tuple[int, bool, float, dict]:
    """Drain diagnostics while recording observed start/PDB-return wall times."""
    times = {}
    began = time.monotonic()
    process = subprocess.Popen(command, env=env, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT, start_new_session=True)
    pending = b""
    timed_out = False
    selector = selectors.DefaultSelector()
    assert process.stdout is not None
    selector.register(process.stdout, selectors.EVENT_READ)
    with logfile.open("wb") as log:
        try:
            while selector.get_map():
                if time.monotonic() - began > timeout:
                    timed_out = True
                    os.killpg(process.pid, signal.SIGKILL)
                    break
                for key, _ in selector.select(0.5):
                    block = os.read(key.fd, 65536)
                    if not block:
                        selector.unregister(key.fileobj)
                        continue
                    log.write(block)
                    log.flush()
                    pending += block
                    while b"\n" in pending:
                        line, pending = pending.split(b"\n", 1)
                        match = re.search(rb"BLINDS_(CASE_START|PDB_DONE|CASE_DONE)=([A-Za-z0-9-]+)", line)
                        if match:
                            stage, ident = (item.decode("ascii") for item in match.groups())
                            times.setdefault(ident, {})[stage] = time.monotonic() - began
            try:
                code = process.wait(timeout=max(1.0, timeout - (time.monotonic() - began)))
            except subprocess.TimeoutExpired:
                timed_out = True
                os.killpg(process.pid, signal.SIGKILL)
                code = process.wait()
        finally:
            selector.close()
            process.stdout.close()
            if process.poll() is None:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
    return code, timed_out, time.monotonic() - began, times


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--capture", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "migration/fixtures/legacy-blinds")
    parser.add_argument("--legacy-source", type=Path, default=ROOT.parent / "gimp-painter-legacy")
    parser.add_argument("--legacy-prefix", type=Path, default=Path("/workspace/shared/gimp-legacy-build/prefix"))
    parser.add_argument("--lock", type=Path, default=Path("/workspace/shared/gimp-painter-build.lock"))
    parser.add_argument("--timeout", type=float, default=300)
    args = parser.parse_args()
    source, prefix, out = args.legacy_source.resolve(), args.legacy_prefix.resolve(), args.output.resolve()
    if (out / "capture-report.json").exists():
        raise SystemExit("Existing evidence is sealed; choose a new --output directory")
    commit = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if commit != COMMIT:
        raise SystemExit("Wrong legacy source commit")
    hashes = {}
    for name in SOURCES:
        hashes[name] = sha(source / name)
        pinned = subprocess.check_output(["git", "-C", str(source), "show", COMMIT + ":" + name])
        if hashlib.sha256(pinned).hexdigest() != hashes[name]:
            raise SystemExit("Legacy oracle source has local changes: " + name)
    gray, coefficients = gray_background(source)
    out.mkdir(parents=True, exist_ok=True)
    inputs, cases = prepare(out, gray)
    (out / "capture-plan.json").write_text(json.dumps(dict(schema_version=1, source_commit=commit,
        background_rgb=BACKGROUND, background_gray=gray, gray_coefficients=coefficients,
        inputs=inputs, cases=cases), indent=2) + "\n")
    print(f"Prepared {len(cases)} old PDB cases; gray background={gray}", flush=True)
    if not args.capture:
        return 0
    executable = prefix / "bin/gimp-2.8"
    plugin_dir = prefix / "lib/gimp/2.0/plug-ins"
    script_dir = prefix / "share/gimp/2.0/scripts"
    binaries = [executable] + [plugin_dir / name for name in ("blinds", "file-png", "script-fu")]
    initializers = [script_dir / name for name in ("script-fu.init", "script-fu-compat.init", "plug-in-compat.init")]
    before_hashes = {str(path.relative_to(prefix)): sha(path) for path in binaries + initializers}
    started = datetime.now(timezone.utc).isoformat()
    with args.lock.open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        with tempfile.TemporaryDirectory(prefix="gimp-legacy-blinds-") as temporary:
            home = Path(temporary)
            for folder in ("profile", "plug-ins", "scripts", "empty", "temp", "cache", "config", "data"):
                (home / folder).mkdir(mode=0o700)
            for path in binaries[1:]:
                (home / "plug-ins" / path.name).symlink_to(path)
            for path in initializers:
                (home / "scripts" / path.name).symlink_to(path)
            system_rc = home / "system-gimprc"
            system_rc.write_text("# Empty isolated system configuration.\n")
            user_rc = home / "profile/gimprc"
            properties = {"plug-in-path": "plug-ins", "script-fu-path": "scripts",
                          "module-path": "empty", "interpreter-path": "empty", "environ-path": "empty",
                          "temp-path": "temp", "swap-path": "temp"}
            user_rc.write_text("".join(f"({key} {quote(home / value)})\n" for key, value in properties.items()))
            env = dict(os.environ)
            env.update(HOME=str(home), GIMP2_DIRECTORY=str(home / "profile"),
                       XDG_CONFIG_HOME=str(home / "config"), XDG_CACHE_HOME=str(home / "cache"),
                       XDG_DATA_HOME=str(home / "data"))
            for key in tuple(env):
                if key.startswith("GIMP_TESTING_") or key in ("GIMP_PLUGIN_DEBUG", "GIMP_PLUGIN_DEBUG_WRAP"):
                    del env[key]
            command = [str(executable), "--no-interface", "--no-data", "--no-fonts", "--no-splash",
                       "--new-instance", "--system-gimprc", str(system_rc), "--gimprc", str(user_rc),
                       "--batch-interpreter=plug-in-script-fu-eval", "-b",
                       "(load " + quote(out / "capture.scm") + ")", "-b", "(gimp-quit 0)"]
            code, timed_out, elapsed, times = run_capture(command, env, out / "capture.log", args.timeout)
            registry_created = (home / "profile/pluginrc").is_file()
    errors = []
    log = (out / "capture.log").read_text(errors="replace")
    for item in inputs:
        try:
            with Image.open(out / item["loaded_png"]) as image:
                if image.mode != item["mode"] or image.size != (item["width"], item["height"]):
                    raise ValueError("mode or dimensions changed")
                data = image.tobytes()
            (out / item["loaded"]).write_bytes(data)
            item["loaded_sha256"] = sha(out / item["loaded"])
            item["load_export_identical"] = data == (out / item["generated"]).read_bytes()
            if not item["load_export_identical"]:
                raise ValueError("loaded/exported pixels differ from generated input")
            if "BLINDS_INPUT_LOADED=" + item["id"] not in log:
                raise ValueError("missing successful input marker")
        except (OSError, ValueError) as error:
            errors.append(item["id"] + ": " + str(error))
    for case in cases:
        observed = times.get(case["id"], {})
        try:
            if set(observed) != {"CASE_START", "PDB_DONE", "CASE_DONE"}:
                raise ValueError("missing actual PDB/PNG completion markers")
            with Image.open(out / case["output_png"]) as image:
                if image.mode != case["mode"] or image.size != (case["width"], case["height"]):
                    raise ValueError("mode or dimensions changed")
                data = image.tobytes()
            (out / case["output"]).write_bytes(data)
            case["output_sha256"] = sha(out / case["output"])
            case["output_png_sha256"] = sha(out / case["output_png"])
            case["observed_pdb_elapsed_seconds"] = observed["PDB_DONE"] - observed["CASE_START"]
            case["observed_case_elapsed_seconds"] = observed["CASE_DONE"] - observed["CASE_START"]
        except (OSError, ValueError) as error:
            errors.append(case["id"] + ": " + str(error))
    if not registry_created:
        errors.append("Fresh isolated plugin registry was not written")
    if before_hashes != {str(path.relative_to(prefix)): sha(path) for path in binaries + initializers}:
        errors.append("Legacy executable or interpreter initializer changed during capture")
    if timed_out or code != 0:
        errors.append(f"Legacy process exit={code}, timed_out={timed_out}")
    report = dict(schema_version=1, status="passed" if not errors else "failed",
                  started_utc=started, finished_utc=datetime.now(timezone.utc).isoformat(),
                  duration_seconds=elapsed, source_commit=commit, source_sha256=hashes,
                  source_worktree_diff_sha256=hashlib.sha256(subprocess.check_output(
                      ["git", "-C", str(source), "diff", "--binary"])).hexdigest(),
                  executable_and_initializer_sha256=before_hashes,
                  capture_script_sha256=sha(Path(__file__)), command=command, exit_code=code,
                  timed_out=timed_out, fresh_isolated_registry=registry_created,
                  profile_policy="New temporary HOME/profile; only bundled blinds/file-png/script-fu and three pinned interpreter initializers",
                  background_rgb=BACKGROUND, background_gray=gray, gray_coefficients=coefficients,
                  timing_policy="Parent-observed stderr marker intervals around old PDB call; includes IPC, not an exclusive kernel benchmark",
                  inputs=inputs, cases=cases, case_count=len(cases), captured_count=sum("output_sha256" in c for c in cases),
                  errors=errors, script_sha256=sha(out / "capture.scm"),
                  log_sha256=sha(out / "capture.log"), fixtures_sha256=sha(out / "fixtures.tsv"))
    restoration = prefix.parent / "restoration-20261002.json"
    if restoration.is_file():
        report["legacy_restoration_report"] = {"filename": restoration.name, "sha256": sha(restoration)}
    (out / "capture-report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{report['status']}: {report['captured_count']}/{len(cases)} captures, {len(errors)} errors", flush=True)
    for error in errors[:10]:
        print(error)
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
