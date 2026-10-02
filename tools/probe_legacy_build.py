#!/usr/bin/env python3
"""Record legacy build prerequisites and optionally run its bootstrap check.

This does not install packages, update a checkout, or claim a successful build.
Use an isolated clean checkout at the pinned revision for --bootstrap.
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess

REV = "afa43fae3e920210146abed514f136fd49f671b5"
TOOLS = ("gcc", "g++", "make", "pkg-config", "autoconf", "automake", "automake-1.11",
         "aclocal", "libtoolize", "intltoolize", "gtkdocize", "xsltproc", "flatpak",
         "flatpak-builder", "docker", "podman")
MODULES = {"babl": "0.1.12", "gegl-0.3": "0.3.0", "glib-2.0": "2.30.2",
           "gtk+-2.0": "2.24.10", "json-glib-1.0": "1.0", "atk": "2.2.0",
           "gdk-pixbuf-2.0": "2.24.1", "cairo": "1.10.2", "pangocairo": "1.29.4"}


def run(argv, cwd=None, env=None):
    try:
        result = subprocess.run(argv, cwd=cwd, env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, timeout=120)
        return {"argv": argv, "exit_code": result.returncode, "output": result.stdout}
    except (OSError, subprocess.TimeoutExpired) as error:
        return {"argv": argv, "exit_code": None, "error": str(error)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--bootstrap", action="store_true")
    args = parser.parse_args()
    source = args.source.resolve()
    revision = run(["git", "rev-parse", "HEAD"], source)
    if revision.get("output", "").strip() != REV:
        parser.error("source must be a checkout of the pinned legacy commit")
    status = run(["git", "status", "--porcelain", "--untracked-files=no"], source)
    if status["exit_code"] != 0 or status.get("output"):
        parser.error("legacy tracked files must be clean before probing")
    report = {"schema_version": 1, "captured_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
              "source_commit": REV, "source_path": str(source), "machine": platform.machine(),
              "os_release": Path("/etc/os-release").read_text(), "tools": {}, "pkg_config": {},
              "legacy_application_executed": False}
    for tool in TOOLS:
        location = shutil.which(tool)
        report["tools"][tool] = {"path": location,
                                  "version": run([tool, "--version"]) if location else None}
    for module, version in MODULES.items():
        report["pkg_config"][module] = {"declared_minimum": version,
                                        "probe": run(["pkg-config", "--modversion", module])}
    if args.bootstrap:
        report["bootstrap"] = run(["sh", "./autogen.sh"], source,
                                   {**os.environ, "NOCONFIGURE": "1"})
        report["bootstrap"]["environment_override"] = {"NOCONFIGURE": "1"}
    report["source_checksums"] = {path: hashlib.sha256((source / path).read_bytes()).hexdigest()
                                  for path in ("configure.ac", "autogen.sh",
                                               "build/flatpak/io.github.seagetch.GIMP-PAINTER.yml",
                                               "build/flatpak/io.github.seagetch.GIMP-PAINTER-BaseApp.yml")}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(f"wrote prerequisite observations to {args.output}; legacy runtime not tested")


if __name__ == "__main__":
    main()
