#!/usr/bin/env python3
"""Extract first-pass C/C++ boundary candidates from the legacy C++ sources."""

import csv
import re
import subprocess
from collections import Counter
from pathlib import Path


REVISION = "afa43fae3e920210146abed514f136fd49f671b5"
OUTPUT = Path("migration/inventory/cpp-call-boundary-candidates.tsv")
C_REFERENCES = Path("migration/inventory/c-call-reference-candidates.tsv")
C_COVERAGE = Path("migration/inventory/c-entry-coverage.tsv")
PATTERNS = (
    ("extern_c", re.compile(r'\bextern\s+"C"')),
    ("c_entry_candidate", re.compile(
        r"^\s*(?:extern\s+\"C\"\s+)?(?:GType|void|gboolean|gint|guint|gchar|"
        r"gpointer|Gimp[A-Za-z0-9_]+)\s*\*?\s*gimp_[A-Za-z0-9_]+\s*\(")),
    ("vfunc_candidate", re.compile(
        r"(?:\b(?:klass|iface|[A-Za-z_]*_class)\s*->|"
        r"\b[A-Z_]+_CLASS\s*\([^)]*\)\s*->)\s*[A-Za-z_]+\s*=")),
    ("vfunc_binding_candidate", re.compile(
        r"\b(?:_override|bind_to_class)\s*\(|\bClass::__\s*\(")),
    ("function_pointer", re.compile(r"\(\s*\*\s*[A-Za-z_]\w*\s*\)\s*\(")),
    ("callback_registration", re.compile(
        r"\b(?:G_CALLBACK|g_cclosure_new|g_signal_connect\w*|g_idle_add\w*|"
        r"g_timeout_add\w*|g_source_set_callback|g_object_weak_ref|"
        r"g_type_add_interface_static|g_type_register_static\w*)\s*\(")),
    ("signal_wrapper_candidate", re.compile(r"(?:\.|->)\s*connect\s*\(")),
    ("delegator_candidate", re.compile(r"\b(?:_D::|Delegators::)?delegator\s*\(")),
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


def cpp_function_definitions(code: str):
    """Find gimp_* definitions even when their return type is on another line."""
    for match in re.finditer(r"\b(gimp_[A-Za-z0-9_]+)\s*\(", code):
        depth = 1
        position = match.end()
        while position < len(code) and depth:
            if code[position] == "(":
                depth += 1
            elif code[position] == ")":
                depth -= 1
            position += 1
        if depth:
            continue
        rest = code[position:]
        opening = re.match(r"\s*(?:G_GNUC_[A-Z_]+\s*)*\{", rest)
        if opening:
            yield match[1], code.count("\n", 0, match.start()) + 1


def statement(lines: list[str], line: int) -> str:
    """Keep a bounded multiline call/assignment for callback target review."""
    pieces = []
    for raw in lines[line - 1:line + 11]:
        pieces.append(raw.strip())
        if ";" in raw or "{" in raw:
            break
    return " ".join(" ".join(pieces).split())[:1200]


def main() -> None:
    paths = [p for p in git("ls-tree", "-r", "--name-only", REVISION, "app").splitlines()
             if p.endswith((".cpp", ".hpp"))]
    rows = []
    for path in paths:
        source = git("show", f"{REVISION}:{path}")
        clean = without_comments(source)
        source_lines = source.splitlines()
        for number, (raw, code) in enumerate(
                zip(source_lines, clean.splitlines()), start=1):
            for kind, pattern in PATTERNS:
                if pattern.search(code):
                    rows.append((path, number, kind, "REVIEW", raw.strip()[:300],
                                 statement(source_lines, number)))
        for _, line in cpp_function_definitions(clean):
            rows.append((path, line, "c_definition_candidate", "REVIEW",
                         source_lines[line - 1].strip()[:300],
                         statement(source_lines, line)))
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    with OUTPUT.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("source", "line", "kind", "status", "legacy_code",
                         "boundary_statement"))
        writer.writerows(rows)
    symbols = sorted(set(match[1] for row in rows
                         if row[2] in ("c_entry_candidate", "c_definition_candidate")
                         if (match := re.search(r"\b(gimp_[A-Za-z0-9_]+)\s*\(", row[4]))))
    references = []
    if symbols:
        command = ["git", "grep", "-n", "-w", "-F"]
        for symbol in symbols:
            command.extend(("-e", symbol))
        command.extend((REVISION, "--", "app"))
        for entry in subprocess.check_output(command, text=True, encoding="utf-8").splitlines():
            _, path, line, code = entry.split(":", 3)
            if not path.endswith((".c", ".h")):
                continue
            found = set(re.findall(r"\bgimp_[A-Za-z0-9_]+\b", code)) & set(symbols)
            for symbol in sorted(found):
                references.append((symbol, path, int(line),
                                   "header" if path.endswith(".h") else "c_source",
                                   "REVIEW", code.strip()[:300]))
    C_REFERENCES.parent.mkdir(parents=True, exist_ok=True)
    with C_REFERENCES.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "source", "line", "kind", "status", "legacy_code"))
        writer.writerows(references)
    with C_COVERAGE.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(("symbol", "cpp_definition_sites", "header_sites",
                         "c_source_sites", "status"))
        for symbol in symbols:
            definitions = [f"{row[0]}:{row[1]}" for row in rows
                           if row[2] == "c_definition_candidate"
                           and re.search(r"\b" + re.escape(symbol) + r"\s*\(", row[4])]
            headers = [f"{row[1]}:{row[2]}" for row in references
                       if row[0] == symbol and row[3] == "header"]
            callers = [f"{row[1]}:{row[2]}" for row in references
                       if row[0] == symbol and row[3] == "c_source"]
            writer.writerow((symbol, ",".join(definitions), ",".join(headers),
                             ",".join(callers), "REVIEW"))
    print(f"{len(paths)} C++ files scanned; {len(rows)} candidates: "
          f"{dict(Counter(row[2] for row in rows))}; "
          f"{len(symbols)} candidate C entry names, {len(references)} C/header references")


if __name__ == "__main__":
    main()
