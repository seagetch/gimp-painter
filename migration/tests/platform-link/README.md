# Original WBS 07.014: native platform link gate

The original 07.014 gate is now complete; see the final native acceptance below.
The first two sections preserve the earlier execution checkpoint and its limits. The supported matrix
is Linux x86_64, Windows x86_64, and macOS arm64/x86_64. The new workflow and
runner must be published before GitHub can execute the other native targets.
The original task remains unchecked until all four results are collected and
source/ABI acceptance is verified. No full GIMP release/platform support is
established by this common bridge test.

`tools/test_painter_platform_link.py` imports the existing foundation source
lists without modifying its runner or historical reports. It builds separate
C11 and C++14 units, archives the three production bridge objects and links with
the C++ compiler. Production exception/no-RTTI/inline-visibility policy stays
C++-only. The existing 69-case runtime suite tests real GObject callbacks and
exception/lifetime behavior. The additional C-main/C++ fixture compares twelve
native layout values and exercises mixed pointer/integer/double/64-bit callback
arguments, GValue transfer, C-visible GError and the actual store close path.

A deliberately exported C control makes dynamic export inspection nonempty.
The consumer C++ fixture uses default visibility, while the real common library
and namespace declarations remain hidden. The positive executable must export
no GimpPainter C++ implementation name; an explicit exported private-name
negative control must be detected in a second binary. Internal C hooks are
required to link by unmangled names; they are not promoted to a public ABI.
ELF uses dynamic nm, PE uses the export-name table, and Mach-O uses Apple's
actual dyld exports inspection rather than treating all nm globals as exports.
Each result also records the native host and executable architecture/format,
compiler targets/versions, GObject/C++ runtime dependencies, binary/archive
hashes, thirty source seals (including the visibility header), commands and
exit codes. Failure reports are retained. No environment dump or credentials
are collected. This is not a sanitizer run.

The first local Linux run in linux-local.json passes the 69 existing tests,
native layout/callback/error fixture, unmangled symbol check and both export
controls. Windows and both macOS targets are not yet executed at this checkpoint.
Four recorder controls additionally reject skipped/TODO/failed/incomplete TAP, retain actual timeout/launch/nonzero diagnostics, and a deliberately stale fourth archive member is absent from the final three-member archive.
Their results cannot be inferred from a cross-compiler, this Linux run, or a
workflow file being present.

The workflow uses the existing public repository's standard hosted runners,
read-only contents permission, checkout without persisted credentials, and
pinned actions. It installs dependencies from the OS package registry,
Homebrew or MSYS2 UCRT64; it creates no external account/grant and accesses no
personal device. Each target preserves its report as a separate CI artifact.
CI enablement/runner access is not assumed until an actual run is observed.

References: [supported GitHub runners](https://docs.github.com/en/actions/reference/runners/github-hosted-runners),
[MSYS2 setup v2.33.0](https://github.com/msys2/setup-msys2/releases/tag/v2.33.0),
[Apple dyld export inspection](https://github.com/apple-oss-distributions/dyld/blob/main/other-tools/dyld_info.cpp),
and [the initial port support policy](../../support-policy.md).

## First native CI result and bounded recorder correction

[Run38026874552](https://github.com/seagetch/gimp-painter/actions/runs/38026874552)
actually started all four hosted targets. Linux passed. Windows passed all69
foundation cases and the mixed ABI fixture, then failed export inspection because
current binutils inserts ordinal-base/hint columns. Both macOS targets compiled
the11 units, then stopped because Apple ar lists its standard __.SYMDEF symbol
index alongside objects. The recorder now recognizes only the known Apple index
forms and both PE export table formats, while still rejecting stale/missing
objects. Mach-O re-exports are also included, not silently ignored. Seven recorder
cases exercise these formats and prior failure controls. Full first-run reports
were downloaded and ZIP/report SHA-256 verified. Linux/macOS source hashes match
repository blobs exactly. All30 Windows source hashes match the exact LF-to-CRLF
checkout transformation; these are recorded individually, not described as raw
hash equality. The bounded details are in ci-attempt-1.json. No Windows/macOS PASS
is inferred from the correction; a new native matrix run is required.

## Explicit native demangler policy

[Run 38027444418](https://github.com/seagetch/gimp-painter/actions/runs/38027444418)
passes every gate on Linux and Windows. Both macOS targets now run all 69
foundation cases and the ABI fixture, then reject the intentional export-leak
control. The raw Mach-O table contains the expected C++ symbol, but the recorder
already removed its object prefix and Darwin's c++filt removed another underscore.
The recorder now passes `-n` explicitly, as documented by
[LLVM](https://releases.llvm.org/20.1.0/docs/CommandGuide/llvm-cxxfilt.html),
so normalized Itanium names retain their required underscore. The existing
positive and deliberate negative compiled binaries test this behavior on each
actual platform. No macOS success is inferred from this source correction.
The full four report snapshots and their checkout transformations were verified;
`ci-attempt-2.json` records the bounded result and exact failing export output.

## Final original acceptance

[Run 38027743135](https://github.com/seagetch/gimp-painter/actions/runs/38027743135)
on source `f2137c9a7d4a9d58f8a007fdc911cf0626da7af5` passed all four
native targets: Linux x86_64, Windows UCRT64 x86_64, macOS arm64 and macOS
x86_64. Each compiled and linked the real minimal common bridge, passed the
same 69 foundation cases, C/C++ layout/callback/error fixture, unmangled C
entry checks, native binary/runtime checks, and positive/negative export controls.
Windows uses its native 4-byte long; the other targets use 8-byte long. Each
compares the C and C++ layouts on that target, rather than requiring unrelated
platform ABIs to be byte-identical.

`native-acceptance.json` binds the exact report snapshots to CI jobs, artifacts
and source commit. The four `native-*.json.gz` files preserve original report
bytes, including commands and failures being absent. All thirty source seals
matched that source snapshot; Windows' exact LF-to-CRLF checkout transformation
is explicitly recorded and rechecked. `tools/check_painter_platform_acceptance.py`
verifies the matrix, report digests, real test output, native identities, controls
and current source. The foundation acceptance checker delegates this OS-specific
row to it; earlier unrelated historical seals are not rewritten.

The sole assigned source duty `legacy-9e61b8aac7871dfbd3cc` is retained and
accepted. `source-duty.json` verifies the pinned configure.ac source/base blobs
and maps its four C++ compiler/C++14 configuration lines to the existing Meson
C/C++ setup and the native executable gate. No assignments are moved or removed.
The independent 34.006 obligation stays open. This closes the original minimal
bridge task, with no claim of a full GIMP build on all platforms, MSVC acceptance,
sanitizers on those runners, tablet behavior or completed platform releases.
## Construction-boundary refresh

[Run 38029643264](https://github.com/seagetch/gimp-painter/actions/runs/38029643264)
on `e2a52c063d4953a28d99ecc9534e23a1eece079f` passed the same four native
targets with all 70 foundation cases, including the separately compiled C11
construction-failure caller added for original 07.016. The ABI, native runtime,
C symbols and positive/negative export controls also pass on every target.

The current index points to new `construction-70-*.json.gz` snapshots. The four
initial `native-*.json.gz` reports retain their original bytes and historical
source seals. All thirty current source seals match the new reports, with the
explicit exact Windows LF-to-CRLF transformation. No production change or broader
application/platform acceptance is inferred from this refresh.
