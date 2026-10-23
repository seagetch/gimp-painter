# Original 04.005: private C++ header containment

The original condition remains “C の translation unit に template・STL が漏れない”.
Private implementation headers stay inside the application and its tests. C
callers use the owning module’s C declarations; adding an internal include
search path does not install or publish a plug-in SDK.

## Source scope

The three header-only legacy Makefile additions list eleven headers. Their
[source-specific mappings](../inventory/cpp-header-boundary.json) preserve the
original hunk identity and separate feature duties. Pixel/brush templates map
to the current private paint headers; GLib ownership, callbacks and bindings
map to the common private machinery; JSON and Soup implementation roles stay
with their feature modules. This does not certify all legacy behavior, all
named type handles, or the independent 04.014/04.015 verification tasks.

The current application has 69 private `.hpp` files: 67 production and two test
headers. No C source or C-facing header directly includes them. Existing
`gimp-parallel.h` and `gimpbrushcore-kernels.h` legitimately contain C++ parts
behind `__cplusplus`; their actual C compiler paths must remain valid.

## Reproducible enforcement

`tools/check_painter_cpp_boundary.py` consumes the two real Meson compilation
and installation databases, with optional HTTP disabled and enabled respectively.
It checks every configured application C and C++ translation unit using its
actual compiler arguments plus syntax-only/dependency recording. A preincluded
language assertion rejects a C unit accidentally compiled by a C++ driver.

C dependencies must contain neither private C++ headers nor C++ standard-library
headers. C++ compilations provide positive use coverage. Installation inspection
rejects both a private header installed directly and a directory containing one.
Meson options and the configuration macro must agree; stale configuration inputs
must be reconfigured. Source/build inputs and the private-header inventory are
checked again after the run.

Negative controls inject a transitive private header whose body is C-compatible,
then a real template body which succeeds in C++ and fails in C. Separate controls
reject a C++ compiler used for a C unit and direct/directory header installation.
Passing the default configuration twice must fail before compilation.

With the existing pinned dependency environment and real generated prerequisites:

```sh
python3 -B tools/check_painter_cpp_boundary.py \
  --build-dir BUILD_DEFAULT --http-build-dir BUILD_HTTP \
  --output-dir OUTPUT --report OUTPUT/report.json
```

The checker compiles syntax only and starts no application or HTTP service. It
does not install files. Generated enum/PDB prerequisites may be prepared in an
isolated source copy when the upstream generators would otherwise rewrite
tracked source; their outputs must match the checked-out source bytes.

## Native result

The [2026-10-08 result](../tests/cpp-header-boundary-native.json) records 2,524 C
and 309 C++ configured commands across the two settings. 2,831 syntax checks
passed; the two explicit optional-test failures below are not syntax passes.
All 67 production private headers and one normal test header were consumed by
C++ units; only the archived brush-settings oracle header is outside configured
compilation. No private/STL header reached a C dependency closure, and neither
2,704-entry install manifest exposed a private header. Six native negative/
positive controls and the wrong-configuration rejection passed. Captured inputs
and the header inventory were unchanged at completion.

The [evidence archive](../tests/cpp-header-boundary-evidence.tar.gz) retains the
full result, compiler commands/databases, configuration and installation
manifests, selected diagnostics and per-member hashes. It contains synthetic
verification data and source/build paths, with no application profile or user
image. [Work-item results](../tests/cpp-header-boundary-work-items.json) change
only the three original implementation rows; all 22,987 other rows are unchanged.

## Existing optional test limitation

`app/operations/tests/test-operations.c` is a non-default Meson target. The
unchanged source at `335c224953530c7a85fdc3144d69f031581ad27f` lacks current
Gimp/GEGL and C library declarations. Its syntax failure is recorded, never
counted as a passing compile. The checker requires that exact baseline source,
the non-default target declaration, successful real C preprocessing, no private
or STL dependencies, and no template/STL tokens in the preprocessed text.
Any source change or boundary failure invalidates this narrow classification.
Repairing this pre-existing test is separate from private-header exposure.

Native Linux evidence does not establish Windows/macOS configuration behavior,
callback ABI, runtime semantics, or completion of the migration. The immutable
04.002 and older feature reports retain their historical hashes and TODO states;
later work is recorded in the canonical task/work ledgers rather than rewriting
those historical snapshots.
