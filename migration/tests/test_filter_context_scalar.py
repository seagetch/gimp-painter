#!/usr/bin/env python3
"""Compare the adapter against the exact extracted, compiled old C function.

This complements, rather than replaces, the real native shadow-merge captures.
The extraction retains the old function and macros verbatim; no translated
formula supplies expected bytes. A bounded four-byte row needs no allocation.
"""
import argparse
from datetime import datetime, timezone
import fcntl
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
COMMIT = "afa43fae3e920210146abed514f136fd49f671b5"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "migration/tests/filter-context-scalar.json")
    parser.add_argument("--work", type=Path, default=Path("/workspace/shared/gimp-filter-context-scalar"))
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    if args.output.exists():
        raise SystemExit("Sealed report exists; select another output")
    legacy = ROOT.parent / "gimp-painter-legacy"
    source = legacy / "app/paint-funcs/paint-funcs.c"
    macros = legacy / "app/paint-funcs/paint-funcs-utils.h"
    for path in (source, macros):
        pinned = subprocess.check_output(["git", "-C", str(legacy), "show", COMMIT + ":" + str(path.relative_to(legacy))])
        if hashlib.sha256(pinned).hexdigest() != sha(path):
            raise SystemExit("Changed old oracle source")
    original = source.read_text()
    start = original.index("#define INT_DIV(a, b)")
    stop = original.index("\n/*  replace the contents", start)
    exact_function = original[start:stop]
    args.work.mkdir(parents=True, exist_ok=True)
    extracted = args.work / "old-replace-inten.hpp"
    extracted.write_text("#include <cstdint>\nusing guchar = std::uint8_t; using guint = unsigned;\n"
                         "using gint = int; using gboolean = int;\nstatic const guchar no_mask = 255;\n" +
                         macros.read_text() + "\n" + exact_function)
    harness = args.work / "compare.cpp"
    harness.write_text(r'''
#include "old-replace-inten.hpp"
#include "filter-context.hpp"
#include <cstring>
#include <iostream>
int main ()
{
  std::uint64_t comparisons = 0;
  const unsigned opacity_values[] = {0, 1, 63, 127, 128, 254, 255};
  for (const auto opacity : opacity_values)
    for (unsigned old_alpha = 0; old_alpha < 256; ++old_alpha)
      for (unsigned new_alpha = 0; new_alpha < 256; ++new_alpha)
        for (unsigned mask = 0; mask < 256; ++mask)
          {
            const guchar input[] = {0, 255, static_cast<guchar>(old_alpha), static_cast<guchar>(old_alpha)};
            const guchar shadow[] = {255, 0, static_cast<guchar>(new_alpha), static_cast<guchar>(new_alpha)};
            const guchar coverage = mask;
            guchar expected[4], actual[4];
            const unsigned bits = mask & 15;
            const gboolean active[] = {bool(bits & 1), bool(bits & 2), bool(bits & 4), bool(bits & 8)};
            replace_inten_pixels (input, shadow, expected, &coverage, opacity, active, 1, 4, 4);
            if (!GimpPainter::filter_replace_inten_row (input, shadow, &coverage, actual, 1, 4, bits, opacity) ||
                std::memcmp (expected, actual, 4))
              {
                std::cerr << "Mismatch: " << opacity << ' ' << old_alpha << ' ' << new_alpha << ' ' << mask << '\n';
                return 1;
              }
            ++comparisons;
          }
  std::cout << comparisons << " exact old-source comparisons passed\n";
}
''')
    executable = args.work / ("compare-sanitized" if args.sanitize else "compare")
    command = ["g++", "-std=c++14", "-g", "-O2", "-I" + str(ROOT / "app/painter"),
               str(harness), str(ROOT / "app/painter/filter-context.cpp"), "-o", str(executable)]
    if args.sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-fno-sanitize-recover=all"]
    log = args.output.with_suffix(".log")
    env = dict(os.environ)
    if args.sanitize:
        # This executor cannot run LeakSanitizer's ptrace-based thread scan.
        # Address/undefined checks remain active; no leak coverage is claimed.
        env.update(ASAN_OPTIONS="detect_leaks=0:abort_on_error=1", UBSAN_OPTIONS="halt_on_error=1")
    with Path("/workspace/shared/gimp-painter-build.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        subprocess.run(command, check=True)
        with log.open("w") as stream:
            result = subprocess.run([str(executable)], env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
    report = dict(status="passed" if result.returncode == 0 else "failed", exit_code=result.returncode,
                  completed_utc=datetime.now(timezone.utc).isoformat(), source_commit=COMMIT,
                  sanitizer=args.sanitize, leak_sanitizer=False,
                  sanitizer_options={key: env.get(key) for key in ("ASAN_OPTIONS", "UBSAN_OPTIONS")},
                  command=command, output=log.read_text(),
                  source_sha256={str(path.relative_to(ROOT)): sha(path) for path in
                                 (ROOT / "app/painter/filter-context.cpp", ROOT / "app/painter/filter-context.hpp")},
                  oracle_sha256={str(path.relative_to(legacy)): sha(path) for path in (source, macros)},
                  extracted_sha256=sha(extracted), harness_sha256=sha(harness), executable_sha256=sha(executable))
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(report["status"], report["output"].strip())
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
