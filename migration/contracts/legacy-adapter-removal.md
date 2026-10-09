# Legacy adapter removal condition (original 05.014)

The removal contract has two separate checks. The executable source gate detects
the retired bridge/pixel API families and unreviewed GObject data registration.
Feature acceptance must additionally connect every old type, vfunc and call-site
obligation to its replacement and runtime evidence, or a proven exclusion.
Passing the source gate does not complete those feature obligations, the remaining
31.014–31.017 cleanup tasks, or platform acceptance.

Run the source gate and its positive/negative controls with:

```
python3 -B tools/check_painter_legacy_adapters.py
python3 -B tools/tests/test_painter_legacy_adapters.py
```

The gate reads application source directly and needs neither a running GIMP nor
network access. `--report FILE` writes the full inspected source/digest/site
result. `--root DIRECTORY` permits isolated regression fixtures. A malformed
source construct, missing review, changed/duplicated site or inconsistent policy
fails; a failed parse is never reported as a clean scan.

## Scope and forbidden families

Scan every non-test file below app with a C/C++ source/header/include suffix:
`.c`, `.h`, `.cpp`, `.cc`, `.cxx`, `.hpp`, `.hh`, `.hxx`, `.inc`, `.h++`, `.C`,
and corresponding `.c.in/.h.in/.cpp.in/.cc.in/.hpp.in` generation inputs. New
untracked files in that scope are included. Source symlinks require explicit
scope review. The 78 registered production C++ paths must remain in the scan;
moving one under a tests directory cannot silently exclude it. The present
population is 2,457 files, including the version-header input. Test oracles and
archived migration evidence are outside production scope.

All potentially active conditional branches are inspected, including optional
HTTP and platform code. Only literal constant conditionals prove a branch
inactive. Four existing upstream TileManager/tile_manager sites inside literal
`#if 0` have exact reviewed fingerprints. They remain reported as dormant;
enabling them or adding a new dormant legacy site fails review. This does not
pretend that retained disabled code is a working port.

The gate rejects:

* GLib's retired C++ bridge namespace; NewGClass/GClassWrapper/IGClass/WithClass,
  ImplBase, DerivedFrom/UseCStructs, IObject/BoundMethod and old value wrappers;
  legacy trait-declaration/private-access macros, g_type_class_add_private and
  bridge headers
* `XxxInterface::cast` lookup and legacy ref acquisition, including storing the
  result before using function-index syntax. Ordinary `std::ref` is allowed.
  One exact `Clone ref(...)` declaration and construction in XCF preservation is reviewed:
  Clone is a `std::unique_ptr`, not the retired free function. This exception
  does not permit other calls named ref
* TileManager/PixelRegion identifiers, tile_manager_/pixel_region_/pixel_regions_
  APIs and their obsolete headers
* An unreviewed get/set/steal/replace/dup data/qdata call, including plain set_data,
  full destroy-notify forms, or taking an API address through an alias

Comments, escaped character/string literals and raw strings are not executable
API uses. Include paths are checked separately. Backslash-newline identifiers,
literal token pasting and comments inside preprocessing directives are handled.
Simple quoted legacy-header macros are rejected at their definition as well.
Ordinary native placement new is not forbidden: gimp-parallel.h uses it to copy
callback objects into GLib storage, with matching destruction, and is unchanged
from the pinned upstream. This is distinct from placing Impl inside a NewGClass
GObject private block.

## Native data ownership exceptions

`legacy-adapter-removal.json` is the reviewed policy. Approval is by exact source
path, operation, complete normalized call fingerprint and multiplicity. Literal
keys, payload expressions and destroy callbacks contribute to the fingerprint.
Changing a key/destructor, adding a second identical call, introducing an alias,
moving to another file, or leaving a stale review fails. There is no blanket
permission for C files, Painter-named keys or all qdata calls.

The baseline is GIMP 3.0.9 commit
`95f6410f25c5186686db7a489d79c1e79187cd41`. Its source blob IDs and SHA-256s are
recorded for independent comparison. Of 681 current data operations, 675 match
that upstream's path and complete call tokens. The remaining six are reviewed
explicitly:

1. BindingStore publication and lookup use its single private key; only the
   common store owns C++ implementations
2. Canvas color selection borrows the native `button` metadata installed by
   gimpcoloreditor.c
3. Compact options borrows the native property-name string installed by
   libgimpwidgets/gimpwidgets-private.c
4. Deferred file-save completion retains a native GFile using the existing
   save/export last-file keys and g_object_unref destruction
5. XCF loading transfers the native effect list with g_steal_pointer and the
   existing xcf_load_free_effects destructor

The two common-store operations account for six calls across these five roles.
Considering mutations alone, 334 of 337 match upstream; the three exceptions
are store publication and the two native C transfers. None establishes a second
C++ Impl registration mechanism. Unchanged call tokens do not prove unchanged
surrounding lifetime semantics; owner/callback changes still need code review
and the relevant runtime tests.

## Acceptance and limits

The test fixture deliberately inserts the retired APIs, stored ref use, unknown
branches, re-enabled disabled code, aliases, changed native keys/payloads,
duplicate/stale policy entries, untracked source suffixes and malformed input.
It also checks legitimate comments/literals, std::ref, native placement new,
reviewed calls and the narrow local-variable exception. Current scan results and
test output are sealed in migration/tests/adapter-removal.

This lexical gate is deliberately bounded. It is not a C++ semantic analyzer or
an arbitrary macro expander. Renamed/reimplemented bridges, indirect generated
includes, opaque code generators, external dependency code, arbitrary function
pointer flows and changes around an unchanged call require the existing build,
ownership and feature review gates. It must not be cited as proof that every
callback is exception-safe, every handle is implemented, or every old feature
has been ported. The separate source/target, C ABI and generated dependency
checks remain required; new generation mechanisms need explicit scope review.

Update exceptions only after comparing the actual native owner, payload,
destroy/transfer behavior and caller lifetime. Changing policy solely to make
the count pass is not a removal proof. The pinned inventories retain their
unfulfilled runtime obligations, and original 05.014 has no direct source-work
ledger rows to reassign or mark complete.
