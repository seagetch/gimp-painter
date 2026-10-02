#!/usr/bin/env python3
"""Capture the four genuine legacy Gaussian PDB aliases, without reading XCF.

Source the existing legacy build environment before --capture. The old binary
and source revision are checked; an existing capture report is never replaced.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from PIL import Image

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--output", type=Path, default=root / "migration/fixtures/legacy-gauss-alias")
parser.add_argument("--legacy-prefix", type=Path, default=Path("/workspace/shared/gimp-legacy-build/prefix"))
parser.add_argument("--legacy-source", type=Path, default=root.parent / "gimp-painter-legacy")
parser.add_argument("--capture", action="store_true")
parser.add_argument("--negative-regions", action="store_true", help="Use tiny canonical/two-radius negative-region probes in a fresh --output")
args = parser.parse_args()
out = args.output.resolve()
if (out / "capture-report.json").exists():
    raise SystemExit("Existing capture is sealed; use a different --output")
if args.capture and (out / "capture.log").exists():
    raise SystemExit("An earlier capture has a log but no sealed report; inspect it before any retry")
out.mkdir(parents=True, exist_ok=True)
(out / ".gitattributes").write_text("*.rgba binary\n*.ya binary\ncapture.log -whitespace\n")
inputs, cases, negative = [], [], []
shapes = (("rgb-odd",9,9,4),("gray-odd",9,9,2),("rgb-even",8,8,4),("gray-even",8,8,2)) if args.negative_regions else (("rgb",9,8,4),("gray",9,8,2),("tile-rgb",67,66,4))
for label, width, height, channels in shapes:
    mode = "RGBA" if channels == 4 else "LA"
    suffix = ".rgba" if channels == 4 else ".ya"
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            rgb = ((x*37+y*61+23)%256,(x*13+y*47+103)%256,(x*83+y*17+191)%256)
            pixels.extend((*rgb,255) if channels == 4 else (rgb[0],255))
    raw, png = "input-"+label+suffix, "input-"+label+".png"
    (out / raw).write_bytes(pixels)
    Image.frombytes(mode,(width,height),bytes(pixels)).save(out / png,compress_level=9)
    inputs.append(dict(label=label,width=width,height=height,channels=channels,mode=mode,raw=raw,png=png))
    for method, base in ((0,"plug-in-gauss-iir"),(1,"plug-in-gauss-rle")):
        triples = [(2.5,1,1)] if label == "tile-rgb" else [
            (radius,h,v) for radius in (0.75,2.5,25) for h,v in ((1,1),(1,0),(0,1),(0,0),(2,-3))]
        doubles = [(2.5,7.25)] if label == "tile-rgb" else [(25,25),(2.5,7.25),(0,25),(-2,3),(0.25,0.75),(1,5)]
        if args.negative_regions:
            pairs = [(-1.1,3),(-2,3),(-5,3),(-17,3),(3,-2),(3,-5),(3,-17)]
            invocations = ((base+"2",pairs),("plug-in-gauss",[(*pair,method) for pair in pairs]))
        else:
            invocations = ((base,triples),(base+"2",doubles))
        for procedure, values in invocations:
            for options in values:
                ident = f"{label}-{procedure.removeprefix('plug-in-')}-" + "-".join(f"{v:g}" for v in options)
                cases.append(dict(id=ident,procedure=procedure,options=options,method=method,width=width,height=height,
                                  channels=channels,input=raw,png=png,output=ident+suffix))
for base in (() if args.negative_regions else ("plug-in-gauss-iir","plug-in-gauss-rle")):
    for radius in (0,-2):
        negative.append(dict(id=f"{base}-{radius}",procedure=base,options=(radius,1,1)))
    for value in (0,-2):
        negative.append(dict(id=f"{base}2-{value}",procedure=base+"2",options=(value,value)))
quote = lambda value: json.dumps(str(value))
def load(filename):
    path = quote(out / filename)
    return f"(let* ((image (car (gimp-file-load RUN-NONINTERACTIVE {path} {path}))) (layer (car (gimp-image-get-active-layer image))))"
def save(filename):
    path = quote(out / filename)
    return f"(file-png-save2 RUN-NONINTERACTIVE image layer {path} {path} 0 9 0 0 0 0 0 0 1)"
script = []
for src in inputs:
    script.append(load(src["png"])+"\n "+save("loaded-"+src["label"]+".png")+
        f'\n (gimp-message (string-append "ALIAS_INPUT={src["label"]} TYPE=" (number->string (car (gimp-drawable-type layer))))) (gimp-image-delete image))')
for case in cases:
    options = " ".join(str(v) for v in case["options"])
    script.append(load(case["png"])+f'\n ({case["procedure"]} RUN-NONINTERACTIVE image layer {options})\n '+
        save(case["id"]+".png")+f'\n (gimp-message "ALIAS_DONE={case["id"]}") (gimp-image-delete image))')
(out / "capture.scm").write_text("\n\n".join(script)+"\n")
for case in negative:
    options = " ".join(str(v) for v in case["options"])
    (out / (case["id"]+".scm")).write_text(load("input-rgb.png")+
        f'\n (gimp-message "ALIAS_REJECT_START={case["id"]}")\n ({case["procedure"]} RUN-NONINTERACTIVE image layer {options})\n (gimp-message "ALIAS_REJECT_UNEXPECTED={case["id"]}") (gimp-image-delete image))\n')
(out / "cases.json").write_text(json.dumps(cases,indent=2)+"\n")
(out / "fixtures.tsv").write_text("# procedure width height channels n-options a b c input output\n"+"".join(
    "\t".join(str(v) for v in (c["procedure"],c["width"],c["height"],c["channels"],len(c["options"]),
        *c["options"],*((0,) if len(c["options"])==2 else ()),c["input"],c["output"]))+"\n" for c in cases))
print(f"Prepared {len(cases)} positive cases, {len(negative)} calling-error probes, {len(inputs)} native inputs")
if not args.capture:
    raise SystemExit(0)
sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
commit = subprocess.check_output(["git","-C",str(args.legacy_source),"rev-parse","HEAD"],text=True).strip()
if commit != "afa43fae3e920210146abed514f136fd49f671b5":
    raise SystemExit("Wrong pinned legacy revision")
executables = [args.legacy_prefix / "bin/gimp-2.8"]+[args.legacy_prefix / "lib/gimp/2.0/plug-ins" / p for p in ("blur-gauss","file-png")]
if sha(executables[0]) != "fefc8fa5190592780b5bd880226a99344268b07074b294431903583e9b5013c8":
    raise SystemExit("Legacy executable does not match the established reference")
home = args.legacy_prefix.parent / ("runtime-gauss-negative" if args.negative_regions else "runtime-gauss-alias")
profile = home / "profile"
profile.mkdir(parents=True,exist_ok=True)
pluginrc = args.legacy_prefix.parent / "runtime-home/profile/pluginrc"
if pluginrc.exists() and not (profile / "pluginrc").exists():
    shutil.copyfile(pluginrc,profile / "pluginrc")
env = dict(os.environ); env.update(HOME=str(home),GIMP2_DIRECTORY=str(profile))
command = [str(executables[0]),"--no-interface","--no-data","--no-fonts","--no-splash","--new-instance",
           "--batch-interpreter=plug-in-script-fu-eval"]
for filename in ["capture.scm"]+[c["id"]+".scm" for c in negative]:
    command += ["-b","(load "+quote(out / filename)+")"]
command += ["-b","(gimp-quit 0)"]
with (out / "capture.log").open("w") as log:
    result = subprocess.run(command,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
log = (out / "capture.log").read_text(errors="replace")
report = dict(schema_version=1,captured_utc=datetime.now(timezone.utc).isoformat(),source_commit=commit,
    entry_point="Actual pinned old Gaussian canonical/alias PDB, whole unselected native RGB/Gray U8 drawable; no XCF reader",
    command=command,environment={k:env[k] for k in ("HOME","GIMP2_DIRECTORY","BABL_PATH","LEGACY_BUILD_ROOT") if k in env},
    executables={str(p):sha(p) for p in executables},exit_code=result.returncode,
    source_sha256=sha(args.legacy_source / "plug-ins/common/blur-gauss.c"),script_sha256=sha(out / "capture.scm"),
    log_sha256=sha(out / "capture.log"),inputs=[],cases=[],negative=[])
errors = []
for src in inputs:
    path = out / ("loaded-"+src["label"]+".png")
    marker = f'ALIAS_INPUT={src["label"]} TYPE={1 if src["channels"]==4 else 3}'
    if marker not in log or not path.exists():
        errors.append("Missing input proof: "+src["label"]); continue
    image = Image.open(path)
    unchanged = image.mode==src["mode"] and image.size==(src["width"],src["height"]) and image.tobytes()==(out / src["raw"]).read_bytes()
    report["inputs"].append({**src,"unchanged":unchanged,"input_sha256":sha(out / src["raw"]),"loaded_png_sha256":sha(path)})
    if not unchanged: errors.append("Changed input: "+src["label"])
for case in cases:
    path = out / (case["id"]+".png")
    if "ALIAS_DONE="+case["id"] not in log or not path.exists():
        errors.append("Missing completion: "+case["id"]); continue
    image = Image.open(path)
    if image.mode != ("RGBA" if case["channels"]==4 else "LA") or image.size != (case["width"],case["height"]):
        errors.append("Wrong native output: "+case["id"]); continue
    (out / case["output"]).write_bytes(image.tobytes())
    report["cases"].append({**case,"input_sha256":sha(out / case["input"]),"output_sha256":sha(out / case["output"]),"png_sha256":sha(path)})
for case in negative:
    segment = log.split("ALIAS_REJECT_START="+case["id"],1)[-1].split("ALIAS_REJECT_START=",1)[0]
    rejected = "ALIAS_REJECT_START="+case["id"] in log and "ALIAS_REJECT_UNEXPECTED="+case["id"] not in log and "invalid input arguments" in segment.lower()
    report["negative"].append({**case,"calling_error":rejected,"script_sha256":sha(out / (case["id"]+".scm"))})
    if not rejected: errors.append("Unverified calling error: "+case["id"])
report.update(case_count=len(report["cases"]),expected_case_count=len(cases),errors=errors,
              fixtures_tsv_sha256=sha(out / "fixtures.tsv"),status="passed" if not errors and result.returncode==0 else "failed")
(out / "capture-report.json").write_text(json.dumps(report,indent=2)+"\n")
print(json.dumps({k:report[k] for k in ("case_count","expected_case_count","status","errors")}))
raise SystemExit(0 if report["status"]=="passed" else 1)
