# Existing C header readability audit (04.003)

The 89 source-specific audit obligations cover 43 legacy paths: 42 C headers
and the paired `gimpcontext.c` implementation. The source ledger identifies every
hunk and retains its immutable source hash. `tools/check_painter_headers.py`
compiles the corresponding current include route twice under C11 and C++14,
with the real application type prerequisites and an explicit C adapter boundary.
The test does not pretend internal headers are dependency-free public headers.

## Findings and resolutions

- `GimpContext::template`, `GimpDrawable::private` and
  `GimpCanvasItem::private` are C++ keywords. Conditional spelling uses
  `template_object` / `priv` for C++, without changing C source spelling,
  storage order, type or ABI. The paired `gimpcontext.c` remains compiled as C;
  it needs no global token replacement or C++ recompilation.
- Template-related function parameter names in `gimpcontext.h`,
  `gimptemplate.h` and `gimpimage-new.h` use `template_object` in both languages.
  Parameter names have no symbol or calling-convention effect.
- All 18 `GIMP_CONFIG_PROP_*` macros now explicitly cast combined flag bits to
  `GParamFlags`, rather than relying on C's implicit integer-to-enum conversion.
  Every macro is expanded by the C and C++ probe; only syntax probing would
  miss this problem. The original bits and argument evaluation counts remain.
- Public libgimpconfig/libgimpwidgets headers are reached through their required
  umbrella headers. Their intentional direct-inclusion guards are preserved.
- Old `gimpbaseconfig.h` no longer exists: its readability route is the modern
  `gimpgeglconfig.h`. Old `gimpcairocolor.h` similarly routes through the modern
  color/widgets umbrella. Configuration fields and color numerical equivalence
  remain feature migration obligations, not an inference from this audit.
- Existing prerequisite declaration order is recorded in the probe prologue.
  No speculative global `extern "C"` wrapping of C++ implementation headers is
  introduced. New painter C headers already use `G_BEGIN_DECLS`/`G_END_DECLS`.
- A pre-existing upstream `const guint` return qualifier warning remains visible
  (`-Wno-error=ignored-qualifiers`); all other enabled warnings are errors.

## Evidence

Run after sourcing `tools/linux-debian13-env.sh`:

```
python tools/check_painter_headers.py
python tools/check_tasks.py
python tools/audit_legacy_granularity.py --check
```

`migration/tests/painter-headers.json` records 88 successful probes: 42 header
routes in each language, both complete macro expansions, and executable C/C++
layout comparisons. The latter compare size and selected field offsets for
GimpContext, GimpContextClass, GimpDrawable and GimpCanvasItem on this Linux ABI.
The source hashes and exact compiler commands are recorded. Actual CloneLayer
and FilterLayer C/C++ app integration tests also compile against these headers.

This completes the existing-header readability audit, not all feature-added
headers, runtime behavior, cross-platform ABI or the remaining build/linkage
WBS. Newly introduced APIs must retain C include/link tests in their own feature
slices. Windows and macOS remain untested here.
