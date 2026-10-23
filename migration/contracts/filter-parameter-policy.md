# Filter parameter semantic policy

This is the bounded Phase B implementation of
`16.010/parameter-semantic-policy`. It separates the accepted four isolated
routes' legacy semantic rules from the current trusted provider's runtime
`GParamSpec` declarations. [The parameter contract](filter-parameter-schema.md)
remains authoritative for later editor, persistence and integrated acceptance
work. This slice changes neither algorithms, precision policy, wire/saved
formats, the scheduler nor the allowlisted route set.

## Authorities and placement

`app/painter/filter-procedure-policy.hpp` contains only plain C++14 data and
functions. `FilterLegacy` names the saved literal, execution alias and public
registration separately, together with admitted scalar domains, inactive request
defaults, typed array counts and explicit flag/narrowing adapters. These defaults
are never used to repair or fill missing saved arguments. The owner and private
request validator consume the same scalar rules; GIMP/GTK/libgimp types stay out
of painter.

`RuntimeMetadata` in `app/core/gimpfilterprocedure-arguments.cpp` independently
pins the trusted bundled provider's current scalar declarations. The existing
name/type/role, read-write flag, context prefix 0/1/2 and complete assignment
checks remain in the core binder. Registration/path/public-hidden identity
checks stay in the helper, using the named adapter identities. Matching a saved
name is not sufficient to authorize an executable or arbitrary PDB call.

| Parameter | Legacy execution / inactive request default | Runtime declaration |
|---|---|---|
| Blinds angle | 0..90 / 0 | 0..90 / 30 |
| Blinds segments | 1..100 / 1 | 1..1024 / 3 |
| Blinds direction / transparency | only integer 1 is vertical; every nonzero flag is true | horizontal/vertical choice and Boolean |
| Small Tiles factor | 0..6 / 2 | hidden 0..6; public 2..6; default 2 |
| Retinex scale | 16..256 / 240 | hidden 16..256; public 16..250; default 240 |
| Retinex nscales/mode/cvar | 0..8 / 3; 0..2 / 0; finite 0..4 / 1.2 | matching numeric domains; mode is uniform/low/high choice |
| Convolution arrays | exactly 25 binary64 coefficients and 5 signed int32 flags | exact boxed element subtype; required counts are semantic constraints |
| Convolution divisor/offset | finite, nonzero divisor; U8 legacy narrowing; 1 / 0 | finite double bounds / 1 and 0; pspec alone does not reject zero divisor |

Sources and numerical details remain case-specific:
[Blinds](filter-process-bridge.md), [Small Tiles](filter-small-tiles.md),
[Retinex](filter-retinex.md), [Convolution](filter-convolution.md).

## Preserved distinctions

- Saved Blinds checks payload slots 3..6, but does not require integer saved
  context slots 0..2. Small Tiles, Retinex and Convolution retain their existing
  stricter typed-prefix checks. The current image-procedure prefix is a separate
  execution ABI and still binds the private helper's objects
- Convolution accepts only the existing 11/12-slot shapes. The twelfth arbitrary
  typed value remains saved and ignored. Counts, boxed types, exact byte lengths
  and nonnull storage are checked before typed reads
- Finite tiny Convolution matrix/offset doubles may become float zero in the U8
  adapter. A divisor becoming float zero is rejected. Native-double mode does
  not narrow. Existing per-pixel intermediate checks remain in the kernel
- Inactive request numeric comparisons still treat positive and negative zero
  as equal where the old request did. Exact-bit save/no-op comparisons and
  copy-validation comparisons remain separate
- Retinex's conservative owner full-image/RGBA admission is explicitly named
  `retinex_owner_geometry`. Execution still validates selected region size,
  native BPP and signed indexing overflow independently. Neither bound replaces
  the other, and scratch accounting remains over the full drawable
- Error ordering/messages, cache retention, existing object leases and callback
  lifetime remain unchanged. Public GLib validation also compares outer GValue
  storage; an in-place boxed mutation still requires the existing exact typed
  content comparison even when that public function returns false

## GLib baseline

Meson now applies the existing minimum 2.70 and maximum 2.76 API policy to both
C and C++. On newer installed headers the missing C++ ceiling had caused
`G_DEFINE_TYPE` to select GLib 2.80 `g_once_init_enter_pointer` and
`g_once_init_leave_pointer`; the rebuilt production worker no longer imports
those symbols. The maximum remains aligned with the existing C conditional-code
policy. Actual GLib 2.70 headers accept this setting by mapping an unknown
MAX_ALLOWED version macro to their current stable version.

The actual minimum-header build also exposed an unconditional assignment to
`GParamSpecClass::value_is_valid` in `libgimpbase/gimpchoice.c`, a member added in
2.74. Its callback declaration, class assignment and implementation are now
version-conditional; the existing `value_validate` callback remains active on
all versions. The ≥2.74 compiled path remains the same (verified after normalizing
diagnostic source file/line IDs for preprocessing comparison). The existing
Clone test subprocess flag now uses the equivalent `(GTestSubprocessFlags) 0`;
the named zero-valued enumerator was also introduced in 2.74. Neither fix
raises the declared minimum or weakens a dependency check. Its stale fixture
expectation of a no-font finalizer critical was removed because the existing
font finalizer was already null-safe; all lifetime assertions remain.

The default-built `test-eevl` explicitly links this build's `libgimpmath` so an
isolated build cannot satisfy that dependency with an incompatible system GIMP
library of the same SONAME. This changes only that test's link dependencies;
`--no-undefined` and `--as-needed` remain enabled.

A separate source/dependency/build prefix completed the full configured default
target set of GIMP 3.0.9 with actual GLib 2.70.0 headers and libraries. Optional
auto features were disabled, libunwind was disabled and painter-http was enabled;
this is not coverage of all optional features or other platforms. All 216
default executable ELF outputs were verified, and test-eevl passed 16/16 cases.
The four core binder groups and Clone lifetime subprocess passed with exact TAP
selection. Per-process loader traces verify canonical GLib/GObject/GIO 2.70
files and hashes, including the Clone subprocess. Strict undefined-symbol
checks remained enabled. The isolated dependency/header/link recipe and the
resolved setup/source failures are recorded in the evidence archive.

## Acceptance and limits

[Phase B acceptance](../tests/filter-parameter-policy/acceptance.json) records:
11,506 pre-refactor/current request domain, reservation and exact-error
comparisons in normal and ASan/UBSan/float-cast-overflow runs; unchanged genuine
old Blinds 320, Small Tiles 196, Retinex 174 and Convolution 296 final/raw outputs;
16 normal native groups; and 10 focused sanitizer native groups. The new Blinds
case verifies typed/raw **model** preservation and cache lineage, not a new XCF
Save/reopen result. All 2,626 tracked fixture files match the accepted base.

After the minimum-API corrections, the four binder groups passed again with the
final current library in normal and focused sanitizer executables, the Clone
subprocess passed, and test-eevl passed 16/16. The ≥2.74 choice token comparison
and bounded minimum-only/test fixes explain why the unchanged full pixel corpus
was not mechanically rerun. The failed Clone expectation and incomplete first
runtime sampling audit are retained separately from accepted results.

The sealed old pixel corpus is read-only evidence, not regenerated from this
implementation. The comparison runner uses the immutable accepted Phase A
header at c7b5f74bf520dbaeb46e93a6ce4424e1a08295ca as its pre-refactor baseline.
Focused sanitizer results do not cover all GIMP/dependency code; LeakSanitizer
remains disabled. Existing test-profile/localization diagnostics and occasional
plug-in Broken-pipe warnings are recorded, not fixed here. Independent GTK
AT-SPI, tablet, other platform, later schema-editor/persistence and full-port
gates stay open.
