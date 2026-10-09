# Generated C headers: original WBS 04.017

The acceptance boundary is repeated inclusion and C linkage of generated C
headers. It does not close the implementation semantics attached to the same
legacy source hunks, platform qualification, or the following static-initializer
and allocator tasks.

## Actual defects and changes

The baseline fails repeated inclusion of the generated authors, language and
BMP Huffman definition headers. Their authoritative generators now emit guards;
the language table's string fields are const-qualified to match their literal
values. Table contents and algorithms are preserved.

The internal PDB template and GLib resource headers previously exposed C++
linkage. The internal header was regenerated from `pdb/app.pl`. A build-machine
resource-compiler adapter adds a C boundary to header output while preserving
C source, binary resource, dependency and diagnostic output. The project-wide
Meson override also covers the unchanged, pinned gimp-data subproject. Native
and simulated-cross fixtures verify that a target-machine compiler is not run.

The welcome data header disagreed with its real C definitions about two const
counts. Its generator now emits matching declarations and a C boundary; the
C output includes its own header to detect future drift.

Bison emits debug declarations before `%code requires`, so a grammar-only
boundary cannot cover its complete interface. The imagemap Meson adapter wraps
the complete generated header, including YYDEBUG declarations, inside its
existing guard. The three tracked fallback headers receive the same boundary.
Grammars, parser C skeletons, token values and union layouts remain unchanged.
No old parser skeleton regeneration is claimed.

## Scope and evidence

The reviewed scope is 63 tracked generated headers plus 40 custom-target headers
and two configured headers in each native configuration. Twelve active resource
headers are included. The optional help-browser resource header is inactive in
both configurations; the common adapter is verified separately with real GLib.
Seventeen stamp headers contain comments only, so repeated inclusion has no ABI
or definition effect. Generated static data is tested with its real prerequisite
context; it is not claimed to be standalone or an intentional repeated-include
fragment.

The registered compile targets retain the original 04.014 boundary and add
126 C/C++ probes for the 63 generated source headers. Both configurations compile
the actual source and dependency headers with no external linkage wrapper:
357 probes with HTTP disabled and 359 with HTTP enabled. The 42 actual build
outputs are additionally double-included in C11 and C++14. Every parser has a
separate translation unit because their token namespaces intentionally overlap.

The 23 controls include original C/C++ and stale-source failures plus missing,
duplicate, unknown and misassigned generated routes. Resource tests use the real
GLib compiler. Parser tests compile and partially link real C providers across
three parsers, fallback/current generation and YYDEBUG off/on. Partial links are
explicitly distinguished from executed, fully linked consumers in the report.

The legacy verification ledger contains 89 obligations under 04.017. Their
current header mappings and actual probes are recorded individually in
`source-duty-review.json`. This completes this boundary test only; sibling
implementation obligations are preserved.

## Reproduction and limits

Use the pinned native dependency environment described in the baseline records.
Configure default and optional-HTTP builds, then build the registered
`painter-public-headers-c` and `painter-public-headers-cpp` archives and run
`painter-public-header-compilation`. The evidence archive includes the exact
commands, generated-output runner, before/after cases and checksummed reports.
`tools/test_resource_compiler.py` runs the isolated resource mechanism tests.
`tools/check_painter_generated_headers.py` verifies this checkpoint's source and
evidence seal.

The current execution is Linux x86_64 with GCC 14.2, Meson 1.7, GLib 2.84.4 and
Bison 3.8.2 for the isolated generation route. Meson's declared 0.61 minimum is
unchanged; this is not a real Windows/macOS/cross-toolchain test. The full native
project selects the tracked parser fallback because Flex/Bison are absent from
its normal build PATH. The separate Meson fixture proves the new generator
adapter's dependency edge, rebuild and clean no-op behavior.

Earlier reports retain their original source hashes. Source-matching checks for
04.014 or other prior checkpoints can therefore identify historical snapshots
after these legitimate generator/registration changes; their fixed hashes are
not replaced with a claim that old execution covered the new source.
