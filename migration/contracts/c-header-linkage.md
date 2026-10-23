# Original 04.004: C-facing header linkage

The original condition remains “C と C++ の両方から include できる”. Application
C interfaces own their C linkage boundaries; callers supply real prerequisite
types, without wrapping the target header in an external `extern "C"` block.
No C implementation is recompiled as C++, and no function signature, enum value,
field order, or installed/private header boundary is changed.

## Bounded source scope

The scope ledger retains all 89 original source duties: 42 legacy header routes
and 13 paired `gimpcontext.c` duties. Removed legacy paths use their documented
GIMP 3 counterparts. Public library headers retain their required umbrella
routes and intentional direct-inclusion guards.

The complete pinned GIMP 3 tree at
`95f6410f25c5186686db7a489d79c1e79187cd41` identifies 49 new application `.h`
files: 42 production interfaces and seven test headers. All 33 existing app
headers modified by the port are included too, including layout-only changes.
This yields 103 route records, 101 distinct targets, and 134 application headers
after following quoted includes and the real type prelude. Private `.hpp`
headers and generated static brush data remain separate responsibilities.

Seventy-seven declaration-bearing headers lacked their own C boundaries.
`G_BEGIN_DECLS` and `G_END_DECLS` now enclose their declarations, after their
include blocks. Each prerequisite header owns its own boundary as well.
Fifteen unguarded headers contain only types, macros or static helpers; they
are included in the syntax checks without inventing external symbols.

The 04.003 readability audit used an explicit outer C adapter. Its passing
results do not prove header-owned linkage. Its report is preserved separately.

## Native acceptance and reproduction

Fresh verification completed on 2026-10-08 after the reconstruction. The
[native result](../tests/header-linkage-native.json),
[build result](../tests/header-linkage-build.json), and
[89 source-duty results](../tests/header-linkage-work-items.json) record the
current acceptance; earlier observations are not substituted for these runs.
The focused checker uses real generated configuration and production libraries:

- Double inclusion of every target under C11 and C++14 without an outer wrapper
- Emitted C/C++ references to all discovered application functions, with exact
  unmangled symbol comparison and actual production archive resolution
- Public XCF includes and the explicitly conditional internal APIs with
  `xcf-private.h` included first
- The supported SDK umbrella routes, representative real library exports, and
  all 18 existing configuration property macros in both languages
- Real C navigation/coordinate/type functions called from C++, and actual C++
  binding/error functions called from a C translation unit
- Selected C/C++ structure sizes and offsets, preserving existing C field names
- Optional HTTP objects built in a separate enabled configuration; no listener,
  token reader, or HTTP runtime behavior is invoked

The navigation negative control must compile the original production `.c` with
the original header and then fail an unwrapped C++ link for all seven functions.
After the header-only change, that exact unchanged C object must link and pass
the seven calls. Mock implementations or surrogate GObject types are not used.

All 237 executed commands met their expected outcomes, including the deliberate
old-header link failure. The 202 direct-include compiles, four internal XCF
compiles, two 18-macro probes and 1,762 application declarations in each language
passed. The 1,758 implemented app APIs and four SDK representatives resolved
against 34 actual application/HTTP archives and the SDK libraries. All 1,762
C/C++ address pairs matched at runtime. Seven navigation calls, coordinate/type
calls, C-to-C++ success/error paths, and eight layout comparisons also passed.
All 218 captured inputs matched before/after execution and an independent final
readback. The normal GUI target completed 1,446 build steps; the separately
enabled HTTP archives completed seven. No GUI or listener was started.

Configure the real default app and separate HTTP build using the existing
[pinned Debian environment](../baseline/debian13/README.md) and
[HTTP supplemental lock](httpd.md). Keep the default HTTP setting disabled.
With their compiler/pkg-config/runtime paths active, run:

```sh
python3 -B tools/check_painter_header_linkage.py \
  --build-dir build-header-contract \
  --http-build-dir build-header-http-contract \
  --output-dir ../header-linkage-results \
  --report ../header-linkage-results/report.json
python3 -B tools/check_tasks.py
python3 -B tools/audit_legacy_granularity.py --check
```

The checker obtains the actual GUI linker command through Ninja unless an exact
`--link-command` JSON is supplied. It replaces only the verified `main` object
in a separate test executable. The original `app/gimp-3.0` is never overwritten.
The baseline Git object above is needed for the genuine negative control.
The [compressed evidence](../tests/header-linkage-native-evidence.tar.gz)
retains the exact full report, selected runtime/negative logs, and original
negative-control source; large reproducible preprocessor listings and binaries
are not duplicated. Scope rows preserve baseline provenance, including their
then-TODO status. Current completion lives in the canonical work ledger and the
89 per-duty results, not those historical status fields.

## Four existing declarations without definitions

These upstream declarations have no implementation or call site in the current
application: `gimp_get_display_by_id`, `gimp_get_display_id`,
`gimp_item_resize_to_image`, and `gimp_editor_popup_menu`. Before this task, their
three header blobs were identical to the pinned upstream blobs:

- `core/gimp-gui.h`: `b4b264f2f288996b25c4765d5e0b80f25ed9f469`
- `core/gimpitem.h`: `f5a2d05eed541155d79460ea4f6ece44b6b3d9ed`
- `widgets/gimpeditor.h`: `4bd9d9368b8ad1315b13d32d76ad5d080ac66d50`

Both C and C++ callers emit the same undefined C names. Thus their absent
implementations are an existing API-availability limitation, not a difference
in including or linking these declarations from C versus C++. The original
04.004 condition does not require inventing upstream function implementations.
Their declaration linkage is still checked; only these four exact names are
excluded from implemented-function linking, and any additional unresolved name
must fail. No stub or deletion conceals this limitation.

## Historical and platform boundaries

This task does not establish complete feature behavior or Windows/macOS ABI.
GCC does not distinguish every C/C++ callback-function type, so native layout
and symbol tests cannot prove all platform calling conventions. Optional HTTP
symbol linking is not an HTTP behavioral or security acceptance result.

The 04.002 preservation check freezes all other 22,799 ledger rows at its own
checkpoint. Completing later duties legitimately changes mutable statuses.
That historical proof remains reproducible at public commit
`aa8e7fad108269e2c1dd5ebcbf78865fb37d1ee9`; its expected digest is not rewritten.
Current WBS and source-granularity validators check the current ledger instead.
Existing Painter/Clone strict source-drift failures remain separate and are not
waived or represented as passing by this header task.
