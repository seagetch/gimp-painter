# SPDX-License-Identifier: GPL-3.0-or-later
"""Find the production RTTI compatibility closure for a focused GIMP harness.

Bridge entries are not the only polymorphic objects: Surface and shared-pointer
support units can also supply COMDAT RTTI to instrumented callers. Conservatively
include all production app C++ units explicitly built with -fno-rtti.
These extra production units get -frtti only. They are not silently claimed as
sanitizer-instrumented. Private archive replacement must cover every old alias.
"""
import json
from pathlib import Path
import shlex


CXX_SUFFIXES = frozenset({".cpp", ".cc", ".cxx", ".c++", ".cp", ".C"})


def bridge_rtti_sources(root, build):
    root = Path(root).resolve()
    selected = set()
    for entry in json.loads((Path(build) / "compile_commands.json").read_text()):
        source = (Path(entry["directory"]) / entry["file"]).resolve()
        if source.suffix not in CXX_SUFFIXES or not source.is_relative_to(root):
            continue
        relative = source.relative_to(root)
        if relative.parts[0] != "app" or "tests" in relative.parts:
            continue
        if "-fno-rtti" not in (entry.get("arguments", []) or shlex.split(entry["command"])):
            continue
        selected.add(relative.as_posix())
    return selected
