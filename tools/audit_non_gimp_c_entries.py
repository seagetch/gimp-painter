#!/usr/bin/env python3
"""Reverse-audit legacy C ABI exports whose names do not start with gimp_."""

import csv
import re
import subprocess
from pathlib import Path

from inventory_call_boundaries import REVISION, without_comments


OUTPUT = Path("migration/inventory/non-gimp-c-entry-review.tsv")
EXPECTED = {
    "features_entry_point": (
        "app/gimp-features.cpp:70", "app/gimp-features.h:32",
        "app/app.c:175", "C_DIRECT", "30.016/feature-entry-point",
        "Registers feature factories against the Gimp owner at startup"),
    "preset_factory_gui_prefs_entry_point": (
        "app/presets/preset-factory-gui.cpp:233",
        "app/presets/preset-factory-gui.h:45",
        "app/dialogs/preferences-dialog.c:2865", "C_DIRECT",
        "28.014/preset-preferences",
        "New GArray is consumed then g_array_unref at preferences-dialog.c:2889"),
    "preset_factory_gui_dialogs_actions_entry_point": (
        "app/presets/preset-factory-gui.cpp:240",
        "app/presets/preset-factory-gui.h:47",
        "app/actions/dialogs-actions.c:335", "C_DIRECT",
        "28.014/preset-dialog-actions",
        "Adds action entries; legacy code reads entry->name before null check"),
    "preset_factory_gui_action_group_entry_point": (
        "app/presets/preset-factory-gui.cpp:247",
        "app/presets/preset-factory-gui.h:46",
        "app/presets/preset-factory-gui.cpp:216", "C_CALLBACK",
        "28.014/preset-action-group",
        "C action factory retains callback; registration and teardown need GIMP 3 audit"),
}


def git(*args):
    return subprocess.check_output(["git", *args]).decode("utf-8", errors="replace")


def source_at(cache, anchor):
    path, number = anchor.rsplit(":", 1)
    assert path in cache, anchor
    lines = cache[path].splitlines()
    assert 0 < int(number) <= len(lines), anchor
    return lines[int(number) - 1]


def definitions(source):
    """Find unqualified function bodies, including split return type lines."""
    for match in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", source):
        name = match[1]
        if source[max(0, match.start() - 2):match.start()] == "::":
            continue
        depth, pos = 1, match.end()
        while depth and pos < len(source):
            depth += (source[pos] == "(") - (source[pos] == ")")
            pos += 1
        if depth or not re.match(r"\s*(?:G_GNUC_[A-Z_]+\s*)*\{", source[pos:]):
            continue
        yield name, source.count("\n", 0, match.start()) + 1


def c_linkage_spans(source):
    """Locate balanced extern C blocks in a comment-masked C header."""
    for match in re.finditer(r'\bextern\s+"C"\s*\{', source):
        pos, depth = match.end(), 1
        while depth and pos < len(source):
            depth += (source[pos] == "{") - (source[pos] == "}")
            pos += 1
        assert depth == 0, match.start()
        yield match.end(), pos - 1


def signature(source, name):
    match = re.search(r"(?m)^[ \t]*(GArray\s*\*|void)\s*\n?[ \t]*" +
                      re.escape(name) + r"\s*\((.*?)\)", source, re.S)
    assert match, name
    result = re.sub(r"\s+", "", match[1])
    args = re.sub(r"\s+", "", match[2])
    return result, "" if args == "void" else args


def main():
    paths = [p for p in git("ls-tree", "-r", "--name-only", REVISION, "app").splitlines()
             if p.endswith((".cpp", ".hpp", ".c", ".h"))]
    raw = {p: git("show", f"{REVISION}:{p}") for p in paths}
    clean = {p: without_comments(s) for p, s in raw.items()}
    cpp_defs = {}
    for path in paths:
        if path.endswith((".cpp", ".hpp")):
            for name, line in definitions(clean[path]):
                if not name.startswith("gimp_"):
                    cpp_defs.setdefault(name, set()).add(f"{path}:{line}")

    # Header-first reverse scan: an export needs a definition and a C-linkage
    # declaration. This deliberately does not depend on the first-pass gimp_
    # symbol inventory or its candidate table.
    exports = {}
    for path in paths:
        if not path.endswith(".h"):
            continue
        header = clean[path]
        for begin, end in c_linkage_spans(header):
            for match in re.finditer(r"\b([A-Za-z_]\w*)\s*\(", header[begin:end]):
                name = match[1]
                if name not in cpp_defs:
                    continue
                start = begin + match.start()
                pos, depth = begin + match.end(), 1
                while depth and pos < end:
                    depth += (header[pos] == "(") - (header[pos] == ")")
                    pos += 1
                if depth or not re.match(r"\s*;", header[pos:end]):
                    continue
                exports.setdefault(name, set()).add(
                    f"{path}:{header.count(chr(10), 0, start) + 1}")

    assert {name: len(sites) for name, sites in exports.items()} == {
        name: 1 for name in EXPECTED}, exports
    assert len([p for p in paths if p.endswith((".cpp", ".hpp"))]) == 71

    rows = []
    for name, (definition, header, use, kind, task, owner) in EXPECTED.items():
        assert definition in cpp_defs[name], (name, cpp_defs[name])
        assert header in exports[name], (name, exports[name])
        assert re.search(r"\b" + name + r"\s*\(", source_at(clean, definition)), definition
        assert re.search(r"\b" + name + r"\s*\(", source_at(clean, header)), header
        assert re.search(r"\b" + name + r"\b", source_at(clean, use)), use
        assert signature(clean[definition.rsplit(":", 1)[0]], name) == signature(
            clean[header.rsplit(":", 1)[0]], name), name
        c_sites = {
            f"{path}:{source.count(chr(10), 0, match.start()) + 1}"
            for path, source in clean.items() if path.endswith(".c")
            for match in re.finditer(r"\b" + name + r"\b", source)
        }
        assert c_sites == ({use} if kind == "C_DIRECT" else set()), (name, c_sites)
        if kind == "C_DIRECT":
            assert use.split(":")[0].endswith(".c")
            assert re.search(r"\b" + name + r"\s*\(", source_at(clean, use))
        rows.append((name, definition, header, use, kind, owner, task, "DONE"))

    # Two similarly named factories have C++-only GIMP::Feature* signatures.
    for name, definition, header, use in (
            ("httpd_get_factory", "app/httpd/httpd-features.cpp:47",
             "app/httpd/httpd-features.h:26", "app/gimp-features.cpp:59"),
            ("httpd_get_gui_factory", "app/httpd/httpd-features-gui.cpp:47",
             "app/httpd/httpd-features-gui.h:26", "app/gimp-features.cpp:61")):
        assert definition in cpp_defs[name]
        assert "GIMP::Feature*" in source_at(clean, header)
        assert name not in exports
        assert re.search(r"\b" + name + r"\b", source_at(clean, use))
        assert not any(re.search(r"\b" + name + r"\b", clean[p])
                       for p in paths if p.endswith(".c"))
        rows.append((name, definition, header, use, "CPP_ONLY",
                     "Feature factory pointer in a conditional C++ build", "-", "DONE"))

    name = "preset_factory_gui_action_group_update"
    definition = "app/presets/preset-factory-gui.cpp:77"
    use = "app/presets/preset-factory-gui.cpp:217"
    assert definition in cpp_defs[name]
    assert re.search(r"\bstatic\s+void\s*\n\s*" + name,
                     clean["app/presets/preset-factory-gui.cpp"])
    assert name not in exports
    assert name in source_at(clean, use)
    rows.append((name, definition, "-", use, "STATIC_C_CALLBACK",
                 "C action factory receives callback and owner is singleton",
                 "01.005/indirect-callback-owners", "DONE"))

    assert "g_array_unref(array);" in raw["app/dialogs/preferences-dialog.c"]
    assert "entry->name" in raw["app/presets/preset-factory-gui.cpp"]
    generated = list(csv.DictReader(
        Path("migration/inventory/generated-gtype-review.tsv").open(
            encoding="utf-8", newline=""), delimiter="\t"))
    assert len(generated) == 6 and all(row["status"] == "DONE" for row in generated)

    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_definition", "header_declaration",
                         "caller_or_registration", "classification", "ownership_or_risk",
                         "followup_task", "status"))
        writer.writerows(rows)
    print(f"{len(paths)} legacy sources scanned; 4 non-gimp C ABI exports, "
          "3 direct calls, 1 C callback, 2 C++-only factories, 1 static callback")


if __name__ == "__main__":
    main()
