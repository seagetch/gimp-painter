# Isolated bundled Filter procedure bridge

This is the first audited native PDB route, `plug-in-blinds`. It does not replace
FilterLayer scheduling with GEGL effects, authorize arbitrary PDB names, or mark
the remaining procedure inventory complete. The existing nine exact native
kernel names continue to use their own routes.

## Ownership and protocol

The UI's existing typed `FilterSlot` owns definitions, generations, dependency
checks, preparation and import. A worker receives only owned scalar options,
pixel rasters and cancellation. No parent GIMP object, Babl object, callback,
saved object identifier or graph crosses the process boundary. The helper
creates its own private GIMP instance, PDB, context, image, drawable and typed
BindingStore-owned progress adapter on its main thread. That adapter currently
serves the native plug-in locally; owner/UI progress forwarding remains open.

The request selector is the numeric allowlisted Blinds enum. No serialized
procedure name, executable, path, script, resource loader or arbitrary plug-in
registry is accepted. The child directly queries the configured bundled Blinds
binary and checks its executable identity and full argument/return signature.
The first route accepts U8 nonlinear RGB/Gray, angles 0..90 and segments 1..100.
As in the old plug-in, orientation 1 is vertical and every other integer is
horizontal; any nonzero transparency argument enables transparent background.

The versioned little-endian framing has a 24-byte header, zero reserved fields,
at most 64 KiB metadata and 128 KiB pixel payloads. Input/output offsets must be
contiguous and totals exact. Publication requires complete input transfer,
exact output, exactly one successful terminal frame, EOF, successful helper
exit, reaping and private-profile cleanup. Partial output remains private.
Protocol stdout is duplicated with CLOEXEC before native initialization; normal
stdout diagnostics are redirected to stderr. Native plug-in child watches drain
before the helper sends its terminal frame.

## Process identity and cancellation

POSIX uses `g_spawn_async_with_pipes_and_fds` with DO_NOT_REAP_CHILD, a dedicated
process group and a parent lifeline mapped to FD 3. The helper marks FD 3 CLOEXEC
before launching native plug-ins. Lifeline EOF makes the helper terminate its
own group. The supervisor sends TERM then KILL only while its group leader is
unreaped. `waitid(WNOWAIT)` observes completion without surrendering identity;
remaining group members are terminated before exact `waitpid` reaping. No group
signal is sent after reap, including an observed ECHILD ownership-loss path.
This requires exclusive reaper ownership; another app-wide `waitpid(-1)` reaper
must not race the WNOWAIT-to-signal interval.

No UI join, pipe wait, spool data I/O, path lookup or child wait is introduced.
The independent worker resolves its helper immediately before spawning, using a
private copy of the owner-prepared process options. Independent lifetime tokens
survive closed layers until worker cleanup ends. Accepted application
Quit requests cancellation and polls those tokens on the owner context every
10 ms. A 5-second deadline is an explicit, diagnostic fallback, preserving the
existing batch exit status. There is no cross-platform process-tree guarantee,
and abrupt-parent-crash profile removal is not claimed.

## Exact samples and phase boundaries

Blinds receives RGBA8 in a private sRGB-labelled surrogate. Native RGB bytes are
copied unchanged; Gray is replicated across RGB so neither ICC conversion nor
modern gray luminance changes the old samples. Output alpha zero restores the
original hidden color bytes, matching the old REPLACE_INTEN shadow merge.
Background byte rounding and Gray luminance coefficients come from pinned old
`gimp_drawable_get_color_uchar`, `gimp_rgb_get_uchar` and
`gimp_rgb_luminance_uchar` at afa43fae3e920210146abed514f136fd49f671b5.

Context-only edits do not start jobs. Background is sampled after the final
input-preparation read, just before native execution can start. The first route
explicitly refuses an unknown or nonempty selection at that point. It uses
already-known selection metadata and never calls an unbounded selection scan.
At import/final publication it independently checks the final selection,
image components and target's own alpha lock; unsupported contexts retain the
previous cache and original definition without eager restart. Full legacy
selection/component merge semantics are a separate acceptance gate.

The trusted swap directory is captured per admitted job. Definition replacement
while a prior worker is cancelling must still copy that directory into the new
spool and options. Mutable configuration is never read from the worker, and old
worker options are not rewritten during replacement.

## Resource and platform limits

All Blinds sizes use the existing bounded file spool. Disabled file swap is an
explicit unsupported context, preserving cache/definition. Admission reserves
parent input/result plus three child rasters and 256 MiB plus audited row work.
That is declared logical admission, not exclusive filesystem allocation or a
universal total-RSS guarantee. The private runtime pins and verifies both native
and GEGL temporary/swap paths, a 32 MiB tile cache, one GEGL worker and disabled
OpenCL. It avoids user/system font loading with an empty private Fontconfig.

Executable resolution starts with the running executable's actual directory,
including when launched through a symlink. Only executables in the configured
build's `app` or `app/tests` directories use that build's helper and Blinds.
Installed hosts use configure-time relative bin-to-libexec and libexec-to-plugin
edges, so a relocated runtime uses its own files even while the original build
remains accessible. Missing or nonexecutable bundled files fail closed; no PATH,
user plug-in registry, saved definition or environment override is consulted.
Path probing occurs on the independent worker or private helper, not the owner
context. Focused instrumentation has an explicit compile-time private overlay;
production has no runtime overlay selector.

Linux relocation is a separate installed-process/pixel/Save acceptance from the
normal build tests; see `../tests/installed-filter-runtime/README.md`. Windows
execution is explicitly unsupported for this route until a platform process
adapter and its cancellation/exit proof exist. The platform-specific path code
alone does not establish Windows or macOS runtime acceptance.

## Evidence boundaries

The fresh 320-case old-PDB corpus and ten verified load/export inputs are under
`migration/fixtures/legacy-blinds`. Its README/report preserve exact source,
executables, command, hashes and old startup diagnostics. Old odd fan widths
leave a background sample even at angle zero; identity smoke geometry must use
even fan widths. Native comparison, live lifecycle, transport, sanitizer and
actual active-Quit evidence are separate gates. Historic lost-run counts are
not evidence for this reconstruction.
