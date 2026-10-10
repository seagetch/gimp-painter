# Original WBS 07.014: native platform link gate

This is an execution checkpoint, not completion of 07.014. The supported matrix
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
