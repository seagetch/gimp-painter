# Common painter foundation verification

These tests cover the shared infrastructure only. No CloneLayer, FilterLayer,
brush, XCF or user-facing feature is claimed to be ported by their success.

Run independently of GIMP (GLib/GObject development files and C/C++14 compilers):

```sh
python3 -B tools/test_painter_foundation.py
python3 -B tools/test_painter_foundation.py --sanitize
```

The standalone runner compiles C translation units using the C compiler, C++
using C++14 with RTTI disabled, archives the production bridge, then links the
C-main executable with the C++ runtime. Every common header is independently
included twice; the C entry headers are tested under both languages. All builds
use warnings as errors. Six negative C++ compilation probes additionally reject
returning a borrowed Impl pointer or reference from initialize/with/read. The integrated Meson target uses the same production
files and tests, under `meson test -C build --suite painter`.

The 31 cases test reference adoption/retention/sinking and copy/move/null;
weak locking before and after owner loss; wrong types and missing/duplicate
slots; multiple slots sharing one implementation type; explicit construction
properties, active properties, repeated GObject dispose and finalization;
reentrant close, failed construction, off-owner-thread rejection; C->C++->C
values and exceptions; GValue/array reassignment; mutex unwinding; signal
block/unblock/after ordering; emitter-first destruction; idle cancellation,
repeat dispatch, callback self-close, and callback exceptions.

Adversarial lifetime cases exercise weak-notify and closure-destroy callbacks
that replace the very wrapper being reset or move-assigned, or delete its owner.
Construction callbacks are prohibited by the production contract; defensive tests
also inject violations (close, duplicate registration, attempted activation and
last-owner-reference release) to check failure cleanup. Finalize fallback reentry
must neither resurrect the GObject nor expose implementation state.

`painter-foundation.json` records the normal run and
`painter-foundation-asan.json` records AddressSanitizer and UndefinedBehaviorSanitizer.
Reports include exact compiler commands, output and hashes of the tested module.
A source change requires regenerating the corresponding report, not reusing a
stale PASS status.

## Separate LeakSanitizer limitation

The initial leak-enabled run completed all its then-current 20 test cases, but
LeakSanitizer exited with a fatal error because this container does not support
its ptrace-based process inspection. The failed output is preserved separately
as `migration/tests/painter-foundation-lsan-blocked.txt`; it is not a leak pass.
The subsequent ASan/UBSan command explicitly sets detect_leaks=0. On a supported
host, run `--sanitize --leak-check`; do not claim that ASan alone proves no leaks.

Platform-specific linking, complete GIMP integration, all 111 audited legacy
callback adapters, worker/UI races and feature acceptance remain separate WBS
requirements. The existing GIMP `save-and-export` failure is a baseline issue,
not a passing painter acceptance test.
