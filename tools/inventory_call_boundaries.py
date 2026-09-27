#!/usr/bin/env python3
"""Extract first-pass C/C++ boundary candidates from the legacy C++ sources."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path


REVISION = "afa43fae3e920210146abed514f136fd49f671b5"
OUTPUT = Path("migration/inventory/cpp-call-boundary-candidates.tsv")
PATTERNS = (
    ("extern_c", re.compile(r'\bextern\s+"C"')),
    ("c_entry_candidate", re.compile(
        r"^\s*(?:extern\s+\"C\"\s+)?(?:GType|void|gboolean|gint|guint|gchar|"
        r"gpointer|Gimp[A-Za-z0-9_]+)\s*\*?\s*gimp_[A-Za-z0-9_]+\s*\(")),
    ("vfunc_candidate", re.compile(
        r"(?:\b(?:klass|iface|[A-Za-z_]*_class)\s*->|"
        r"\b[A-Z_]+_CLASS\s*\([^)]*\)\s*->)\s*[A-Za-z_]+\s*=")),
    ("function_pointer", re.compile(r"\(\s*\*\s*[A-Za-z_]\w*\s*\)\s*\(")),
    ("callback_registration", re.compile(
        r"\b(?:G_CALLBACK|g_cclosure_new|g_signal_connect\w*|g_idle_add\w*|"
        r"g_timeout_add\w*|g_source_set_callback|g_object_weak_ref|"
        r"g_type_add_interface_static|g_type_register_static\w*)\s*\(")),
)


def git(*args: str) -> str:
    return subprocess.check_output(["git", *args], text=True, encoding="utf-8")


def without_comments(source: str) -> str:
    """Mask C/C++ comments while retaining line positions and string contents."""
    out = []
    i = 0
    state = "code"
    while i < len(source):
        ch = source[i]
        nxt = source[i:i + 2]
        if state == "code":
            if nxt == "//":
                out.extend("  ")
                i += 2
                state = "line"
                continue
            if nxt == "/*":
                out.extend("  ")
                i += 2
                state = "block"
                continue
            if ch in "\"'":
                state = ch
        elif state == "line":
            if ch == "\n":
                state = "code"
            else:
                ch = " "
        elif state == "block":
            if nxt == "*/":
                out.extend("  ")
                i += 2
                state = "code"
                continue
            if ch != "\n":
                ch = " "
        elif ch == "\\" and i + 1 < len(source):
            out.extend(source[i:i + 2])
            i += 2
            continue
        elif ch == state:
            state = "code"
        out.append(ch)
        i += 1
    return "".join(out)


def main() -> None:
    paths = [p for p in git("ls-tree", "-r", "--name-only", REVISION, "app").splitlines()
             if p.endswith((".cpp", ".hpp"))]
    rows = []
    for path in paths:
        source = git("show", f"{REVISION}:{path}")
        for number, (raw, code) in enumerate(
                zip(source.splitlines(), without_comments(source).splitlines()), start=1):
            for kind, pattern in PATTERNS:
                if pattern.search(code):
                    rows.append((path, number, kind, "REVIEW", raw.strip()[:300]))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("source", "line", "kind", "status", "legacy_code"))
        writer.writerows(rows)
    print(f"{len(paths)} C++ files scanned; {len(rows)} candidates: "
          f"{dict(Counter(row[2] for row in rows))}")


if __name__ == "__main__":
    main()
