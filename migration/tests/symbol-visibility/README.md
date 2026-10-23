# Original WBS 04.012: native ELF visibility

The final native gate passes in both configurations. The default GUI, console,
and worker lose respectively 543, 510, and 502 private C++ exports; HTTP-enabled
outputs lose 617, 587, and 502. Every baseline nonmangled executable export is
preserved. Both configurations retain byte-identical nine-library SDK and
nine-module controls, with no new compiler warnings.

`report.json` summarizes the result. `evidence.tar.gz` preserves exact baseline
and verified export sets, raw readelf/nm output, compiler commands, ordinary
Meson manifests, guarded-generator checks, source hashes, native logs, and
diagnosed intermediate failures. `evidence-manifest.json` hashes every archived
member. Generic/upstream C++ removals are listed separately in
`default-removed-other-cpp.json` and `http-removed-other-cpp.json`.

This gate examines the final linked GUI, console, and isolated filter-worker
executables in both the default and HTTP-enabled native Linux configurations.
Object-file visibility alone is insufficient because GIMP's normal executable
link uses `--export-dynamic`.

`tools/check_painter_symbol_visibility.py` reads existing Meson outputs. It does
not regenerate, compile, install, run GIMP, start an HTTP listener, or access a
user profile. `readelf --dyn-syms --wide` and `nm -D --defined-only` must agree on
the complete defined dynamic export set. Undefined imports are excluded; weak,
GNU-unique, protected, and versioned exports are retained.

The private ABI classifier covers all Painter namespace families, including
template instantiations and typeinfo containing those private types, private
`_XcfPainterSave` members and nested types, C++ containers of the app-only
`GimpFilterLayerSnapshot`, `GimpFilterArgumentPatch`, and
`_GimpFilterArgumentSpec` records, plus the two translation-unit-local GType
functions found by source audit. Generic
libstdc++ exports and upstream C++ types are not classified as Painter ABI.

The before/after comparison also requires:

- Every baseline nonmangled executable export remains available
- The four representative C functions from the upstream `.cc` files remain
  exported. All nine original app `.cc` compile vectors remain identical except
  for at most the explicitly permitted `-fvisibility-inlines-hidden` switch
- All C compile vectors remain identical, across the whole configuration
- All nine installed libgimp SDK libraries remain byte-identical, with exactly
  the same defined dynamic exports
- All nine enabled C GModule libraries remain byte-identical, with exactly the
  same exports, including `gimp_module_query` and `gimp_module_register`
- The complete install manifest remains identical and contains no installed
  private app headers, C++ headers, or app archives

The actual checks use every baseline C export, rather than relying only on the
four representative upstream names. App-internal C headers and static archives
do not constitute an installed public SDK.

The C++ inline switch also affects generic or upstream inline C++ emissions in
the same translation units. These removals are separately enumerated under
`removed_other_cpp`; they are not presented as private Painter symbols or as C
ABI preservation. The exception to exact `.cc` flags permits only this one
added switch and cannot accept other flag changes, source movement, or removed
compile commands.

## Reproduce the export inspection

Build and retain a baseline before applying the visibility change. The output
directory must be new and outside the native Meson build directory.

```sh
python3 tools/check_painter_symbol_visibility.py --self-test
python3 tools/check_painter_symbol_visibility.py \
  --configuration default --build-dir /path/to/build-default \
  --output /path/to/baseline-default

# After applying the change and rebuilding with the normal Meson link recipes:
python3 tools/check_painter_symbol_visibility.py \
  --configuration default --build-dir /path/to/build-default \
  --baseline /path/to/baseline-default/report.json \
  --output /path/to/final-default
```

Repeat with `--configuration http` and the HTTP-enabled build. In addition to
the three app executables, build all shared libraries under `libgimp*` and all
enabled shared modules under `modules` listed in Meson's `intro-targets.json`.
Missing actual ELF outputs fail inspection; they are not silently skipped.

## Layout and bounded runtime checks

`filter-record-layout.c` is compiled as C and C++ using original and final
production compiler vectors and the exact original/final FilterLayer headers.
All eight runs agree on size, alignment, and all 15 field offsets of the three
annotated records. On the tested x86_64 ABI, the records are 32, 16, and 64 bytes,
each aligned to 8 bytes. C++ also asserts standard layout. These are native
observations, not hardcoded cross-platform layout requirements.

Both rebuilt foundation executables pass all 39 cases. GUI and console
`--version` exits succeed before profile/GUI initialization. The worker has no
version option: its expected exit 125 proves only its loader and early argument
rejection, before worker protocol initialization. A temporary HOME/XDG/GIMP
directory and absent display/session-bus variables isolate each run. GUI
`--version` creates an empty XDG data directory; no files or GIMP profile
directory are created. The original harness's overly strict zero-directory
failure and the corrected bounded rerun are preserved explicitly.

Tracked enum/PDB outputs require the separate guarded-generator build workflow:
redirect only the 17 known source-writing generator recipes to isolated source
copies, verify regenerated bytes against the tracked originals, and preserve
every compiler and linker recipe verbatim. The evidence includes the exact
baseline/final guard checks and source hashes. It does not claim that inspecting
exports exercises any application feature or establishes Windows/macOS ABI
behavior.
