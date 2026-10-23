# Layer/canvas signal lifetime acceptance (07.008)

Actual registered native binaries, not private `/tmp` links, were built and run
on 2026-10-21. Authenticated loopback Xvfb verified 1080×1920×24 and rejected an
empty authority file. The canvas fixture enables on-canvas UI. Actual rendered
UI screenshots are packaged separately by the portrait acceptance worker; these
logs are lifecycle assertions, not visual-equivalence evidence.

| Suite | Normal | Bounded ASan/UBSan | Unchanged timeout |
|---|---|---|---|
| painter-layer-ui | 70/70, 96.918 s | 70/70, 145.926 s | 180 s |
| painter-canvas-ui | 107/107, 28.498 s | 107/107, 48.268 s | 60 s |

No skips or timeout changes. Full canvas107 includes all newly registered signal
cases; a separate focused run is not needed to establish their execution.
`report.json` fixes five development/tested source hashes and the seven new test
paths. `normal-tests.json` preserves the full Meson stdout/stderr/results and
commands, with inherited environment removed. Normal execution command:

    python3 tools/run-painter-xvfb.py meson test -C BUILD --no-rebuild \
      --num-processes 1 --print-errorlogs painter-layer-ui painter-canvas-ui

Sanitizer binaries were built by `migration/tests/build_painter_ui_sanitizers.py`
and `migration/tests/build_canvas_ui_sanitizers.py`. The two build reports list
all exact instrumented source paths, hashes, compiler/link commands, executable
hashes, and unchanged-source checks. Layer scope: 15 instrumented files and 59
RTTI-compatibility-only files. Canvas scope: 62 instrumented files, including
float-cast-overflow checks. GTK and remaining core/dependencies are not fully
instrumented. Leak detection is disabled; no LSan or whole-process claim.
Private instrumented archives leave normal production objects untouched.

Sanitizer execution used the real Meson test environment and the same registered
timeouts under the portrait runner, with
`ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No error was suppressed.

Prior failed fixture attempts are retained as `preliminary-*`, not counted as
passing evidence. A synthetic preset needed a real internal resource identifier;
a direct opacity setter needed a lease on its actual GtkAdjustment emitter
through trailing native notify after synchronous owner close. The lease does
not retain the closed Impl. An earlier private `/tmp` executable could not find
its executable-relative filter worker; the proper registered binary passed all
70 cases. These were fixture/setup corrections, not production changes.

Packaging strips inherited environment and replaces machine workspace prefixes
with `<native>`, `<checkout>`, and `<dependencies>`. Runtime temporary fixture
paths in ordinary diagnostics are retained. Binary/source hashes are unchanged.
Log hashes in original reports identify original raw logs; packaged files have
separate hashes in `artifact-sha256.json`. No raw environment is distributed.
This evidence covers modern mapped signal-owner routes, not historical visual
parity or completion of the whole port/111-vfunc exception gate.
