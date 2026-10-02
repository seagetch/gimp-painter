#!/usr/bin/env python3
"""Capture native gray bytes from the existing pinned legacy GIMP executable.

No XCF reader, source patch, rebuilding, or replacement-kernel oracle is used.
Source the existing legacy build environment before --capture. An existing
capture report is never overwritten; choose a new output directory to repeat.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, default=ROOT / "migration/fixtures/legacy-gray-filter")
parser.add_argument("--legacy-prefix", type=Path, default=Path("/workspace/shared/gimp-legacy-build/prefix"))
parser.add_argument("--legacy-source", type=Path, default=ROOT.parent / "gimp-painter-legacy")
parser.add_argument("--capture", action="store_true")
args = parser.parse_args()
out = args.output.resolve()
if (out / "capture-report.json").exists():
    raise SystemExit("Existing evidence is sealed; choose a different --output to repeat")
out.mkdir(parents=True, exist_ok=True)
(out / ".gitattributes").write_text("*.ya binary\n*.y binary\n")
inputs = {}
cases = []

def make_input(label, width, height, channels, data):
    mode = "LA" if channels == 2 else "L"
    suffix = ".ya" if channels == 2 else ".y"
    filename = "input-" + label + suffix
    (out / filename).write_bytes(data)
    Image.frombytes(mode, (width, height), data).save(out / ("input-" + label + ".png"), compress_level=9)
    inputs[label] = dict(label=label, width=width, height=height, channels=channels,
                         mode=mode, raw=filename, png="input-" + label + ".png")

def edge(label, modes, wraps, amount=1.75):
    src = inputs[label]
    for mode in modes:
        for wrap in wraps:
            ident = f"edge-{label}-m{mode}-w{wrap}-a{amount:g}"
            cases.append(dict(id=ident, procedure="plug-in-edge", width=src["width"], height=src["height"],
                              channels=src["channels"], a=amount, b=wrap, c=mode, input=src["raw"],
                              output=ident + (".ya" if src["channels"] == 2 else ".y")))

def gauss(label, radii):
    src = inputs[label]
    for horizontal, vertical in radii:
        for method in (0, 1):
            ident = f"gauss-{label}-h{horizontal:g}-v{vertical:g}-m{method}"
            cases.append(dict(id=ident, procedure="plug-in-gauss", width=src["width"], height=src["height"],
                              channels=src["channels"], a=horizontal, b=vertical, c=method, input=src["raw"],
                              output=ident + (".ya" if src["channels"] == 2 else ".y")))

for width, height in ((5, 4), (1, 1), (1, 5), (5, 1), (67, 66)):
    label = f"{width}x{height}"
    data = bytes(value for y in range(height) for x in range(width)
                 for value in ((x * 37 + y * 61 + 23) % 256,
                               (0, 1, 63, 127, 191, 254, 255)[(x + 3*y) % 7]))
    make_input(label, width, height, 2, data)
    if label == "5x4":
        edge(label, range(6), (1, 2, 3))
        gauss(label, ((25, 25), (2.5, 7.25), (0, 25), (1, 5)))
    elif width == 67:
        edge(label, (0, 5), (1, 3), 2.0)
        gauss(label, ((2.5, 7.25),))
    else:
        edge(label, (0, 5), (1, 2, 3))
        gauss(label, ((25, 25), (0.25, 0.75)))
for label, alpha in (("opaque", lambda i: 255), ("lowalpha", lambda i: i % 2), ("transparent", lambda i: 0)):
    make_input(label, 9, 8, 2, bytes(v for i in range(72) for v in ((i * 137) % 256, alpha(i))))
    edge(label, (0,), (2,), 2.0)
    gauss(label, ((25, 25), (2, 2)) if label == "opaque" else ((25, 25),))
make_input("noalpha", 9, 8, 1, bytes((i * 137) % 256 for i in range(72)))
edge("noalpha", (0, 5), (2,), 2.0)
gauss("noalpha", ((25, 25),))

quote = lambda value: json.dumps(str(value))
def load(name):
    path = quote(out / name)
    return f"(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE {path} {path}))) (layer (car (gimp-image-get-active-layer image))))"
def save(name):
    path = quote(out / name)
    return f"(file-png-save2 RUN-NONINTERACTIVE image layer {path} {path} 0 9 0 0 0 0 0 0 1)"
script = ["; Genuine pinned legacy PDB capture; channel-native Gray/Gray-alpha PNGs."]
for src in inputs.values():
    script.append(load(src["png"]) + "\n " + save("loaded-" + src["label"] + ".png") +
                  f'\n (gimp-message (string-append "GRAY_INPUT_LOADED={src["label"]} TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))\n (gimp-image-delete image))')
for case in cases:
    image_name = Path(case["input"]).with_suffix(".png").name
    script.append(load(image_name) +
                  f'\n ({case["procedure"]} RUN-NONINTERACTIVE image layer {case["a"]} {case["b"]} {case["c"]})\n ' +
                  save(case["id"] + ".png") +
                  f'\n (gimp-message "GRAY_CASE_DONE={case["id"]}") (gimp-image-delete image))')
(out / "capture.scm").write_text("\n\n".join(script) + "\n")
(out / "cases.json").write_text(json.dumps(cases, indent=2) + "\n")
(out / "inputs.json").write_text(json.dumps(list(inputs.values()), indent=2) + "\n")
(out / "fixtures.tsv").write_text("# procedure width height channels a b c input output\n" +
    "".join("\t".join(str(case[key]) for key in ("procedure", "width", "height", "channels", "a", "b", "c", "input", "output")) + "\n" for case in cases))
indexed = Image.new("P", (5, 4))
indexed.putpalette([v for i in range(256) for v in (i, (i*7) % 256, (i*19) % 256)])
indexed.putdata([i % 4 for i in range(20)])
indexed.save(out / "input-indexed.png", compress_level=9)
for procedure in ("plug-in-edge", "plug-in-gauss"):
    options = "2 2 0" if procedure == "plug-in-edge" else "25 25 0"
    (out / (procedure + "-indexed.scm")).write_text(load("input-indexed.png") +
        f'\n (gimp-message (string-append "INDEXED_PROBE={procedure} TYPE=" (number->string (car (gimp-drawable-type layer))) " BASE=" (number->string (car (gimp-image-base-type image)))))\n ({procedure} RUN-NONINTERACTIVE image layer {options})\n (gimp-message "INDEXED_UNEXPECTED_SUCCESS={procedure}") (gimp-image-delete image))\n')
print(f"Prepared {len(cases)} cases and {len(inputs)} native-gray inputs")
if not args.capture:
    raise SystemExit(0)
commit = subprocess.check_output(["git", "-C", str(args.legacy_source), "rev-parse", "HEAD"], text=True).strip()
if commit != "afa43fae3e920210146abed514f136fd49f671b5":
    raise SystemExit("Wrong legacy source commit")
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
executables = [args.legacy_prefix / "bin/gimp-2.8"] + [args.legacy_prefix / "lib/gimp/2.0/plug-ins" / name for name in ("edge", "blur-gauss", "file-png")]
if sha(executables[0]) != "fefc8fa5190592780b5bd880226a99344268b07074b294431903583e9b5013c8":
    raise SystemExit("Installed GIMP does not match the existing genuine reference")
home = args.legacy_prefix.parent / "runtime-gray-filter"
profile = home / "profile"
profile.mkdir(parents=True, exist_ok=True)
old_pluginrc = args.legacy_prefix.parent / "runtime-home/profile/pluginrc"
if old_pluginrc.exists() and not (profile / "pluginrc").exists():
    shutil.copyfile(old_pluginrc, profile / "pluginrc")
env = dict(os.environ)
env.update(HOME=str(home), GIMP2_DIRECTORY=str(profile))
command = [str(executables[0]), "--no-interface", "--no-data", "--no-fonts", "--no-splash", "--new-instance",
           "--batch-interpreter=plug-in-script-fu-eval"]
for name in ("capture.scm", "plug-in-edge-indexed.scm", "plug-in-gauss-indexed.scm"):
    command += ["-b", "(load " + quote(out / name) + ")"]
command += ["-b", "(gimp-quit 0)"]
with (out / "capture.log").open("w") as log:
    result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
log = (out / "capture.log").read_text(errors="replace")
report = dict(schema_version=1, captured_utc=datetime.now(timezone.utc).isoformat(), source_commit=commit,
              entry_point="Actual legacy Edge/Gauss PDB; native Gray/Gray-alpha channels, whole unselected drawable; no XCF reader",
              command=command, environment={k: env[k] for k in ("HOME", "GIMP2_DIRECTORY", "BABL_PATH", "LEGACY_BUILD_ROOT") if k in env},
              executables={str(path): sha(path) for path in executables}, exit_code=result.returncode,
              script_sha256=sha(out / "capture.scm"), log_sha256=sha(out / "capture.log"),
              source_sha256={name: sha(args.legacy_source / "plug-ins/common" / name) for name in ("edge.c", "blur-gauss.c")},
              inputs=[], cases=[], indexed=[])
errors = []
for src in inputs.values():
    expected_type = 3 if src["channels"] == 2 else 2
    marker = f'GRAY_INPUT_LOADED={src["label"]} TYPE={expected_type} BASE=1'
    png = out / ("loaded-" + src["label"] + ".png")
    if marker not in log or not png.exists():
        errors.append("Missing native-gray input proof: " + src["label"]); continue
    image = Image.open(png)
    if image.mode != src["mode"] or image.size != (src["width"], src["height"]):
        errors.append("Unexpected native input mode/dimensions: " + src["label"]); continue
    data = image.tobytes()
    loaded_raw = out / Path(src["raw"].replace("input-", "loaded-"))
    loaded_raw.write_bytes(data)
    unchanged = data == (out / src["raw"]).read_bytes()
    report["inputs"].append({**src, "loaded_mode": image.mode, "input_sha256": sha(out / src["raw"]),
                              "loaded_sha256": sha(loaded_raw), "unchanged": unchanged})
    if not unchanged: errors.append("Input bytes changed: " + src["label"])
for case in cases:
    png = out / (case["id"] + ".png")
    if "GRAY_CASE_DONE=" + case["id"] not in log or not png.exists():
        errors.append("Missing completion: " + case["id"]); continue
    image = Image.open(png)
    if image.mode != ("LA" if case["channels"] == 2 else "L") or image.size != (case["width"], case["height"]):
        errors.append("Unexpected native output mode/dimensions: " + case["id"]); continue
    data = image.tobytes()
    (out / case["output"]).write_bytes(data)
    report["cases"].append({**case, "png_mode": image.mode, "input_sha256": sha(out / case["input"]),
                             "output_sha256": sha(out / case["output"]), "png_sha256": sha(png)})
for procedure in ("plug-in-edge", "plug-in-gauss"):
    marker = f"INDEXED_PROBE={procedure} TYPE=4 BASE=2"
    rejected = marker in log and "INDEXED_UNEXPECTED_SUCCESS=" + procedure not in log and bool(re.search(r"execution[^\n]*" + re.escape(procedure), log, re.I))
    report["indexed"].append(dict(procedure=procedure, rejected=rejected, script_sha256=sha(out / (procedure + "-indexed.scm"))))
    if not rejected: errors.append("Unverified indexed rejection: " + procedure)
report.update(case_count=len(report["cases"]), expected_case_count=len(cases), errors=errors,
              fixtures_tsv_sha256=sha(out / "fixtures.tsv"), status="passed" if not errors and result.returncode == 0 else "failed")
(out / "capture-report.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({k: report[k] for k in ("case_count", "expected_case_count", "status", "errors", "indexed")}))
raise SystemExit(0 if report["status"] == "passed" else 1)
