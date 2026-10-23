# Original 04.018: explicit C++ initialization

The current application and Filter worker have no C++ object initialization root
that can register a GType or call GTK before explicit application startup. No
production initialization-order defect was found, so this change adds a checked
native invariant and its non-vacuous controls rather than an unnecessary runtime
registration mechanism.

## Current native proof

Both configurations were rebuilt from the current source: 73 production C++
translation units with HTTP disabled and 78 with HTTP enabled. Fresh Meson target
ownership and the compiler database match the complete application C++ source
population. The Filter worker and every private library object are covered,
including archive members that a particular final executable might omit. A
production source separately compiled into a test is excluded by actual target
ownership, not merely its source filename.

Every real object is ELF relocatable, non-LTO and free of initialization roots:
INIT_ARRAY/PREINIT_ARRAY, .init/.ctors and priority variants, compiler-generated
initializer symbols, dynamic TLS initializer symbols and IFUNC resolvers. Local
static guards are allowed. All six current GUI/console/worker link recipes are
also checked for absence of an explicit init-function override. This is a fresh
object/input proof; full application executables were not relinked or interactively
run for this task.

Compiler dependency records seal 463 source inputs in the default configuration
and 473 with HTTP, plus four actual configured/generated inputs each. Eight
native controls distinguish safe constant and function-local initialization from
namespace, arbitrarily named and prioritized constructors. Their real GObject
registration and GTK type getters execute; counters assert whether they ran
before main or only on the first explicit lazy call. IFUNC/TLS controls check
inspection without asserting that all lazy TLS use is inherently unsafe. LTO
objects are refused rather than silently treated as ordinary native objects.
Nine metadata controls confirm complete ownership and reject missing/duplicate
objects, missing registrations and HTTP configuration drift.

## Valid lazy and explicit routes

The scheduler admission pool, FairDispatcher, dissolve PRNG seeds and XCF mapping
state are function-local. Their constructors run on explicit use. TypeTraits and
SlotSpec obtain GTypes through getters; no namespace variable caches a GType by
calling a getter. Global atomics, pointer arrays, generated metadata tables and
upstream stateless paint dispatch objects are constant/trivial initialization.

The initialization phase starts with controlled application startup. In
particular, gimp_constructed calls gimp_paint_init during gimp_new, before the later
function named gimp_initialize. This existing explicit paint registration remains
valid. HTTP is created explicitly after initialize/restore and conditionally
attaches GUI navigation. No service/listener is globally constructed.

The legacy features_entry_point was itself an explicit C entry. Current factories,
paint/tool/action registration and HTTP teardown have documented owner-specific
routes. Comprehensive lifecycle equivalence remains separately tracked under
30.016/feature-entry-point. There are zero canonical source-ledger rows assigned
to 04.018; other source obligations are preserved, and none are invented here.

## Reproduce and scope limits

After compiling the production object inputs, run:

    python3 tools/check_cpp_static_initialization.py --build BUILD --report REPORT.json
    python3 tools/test_cpp_static_initialization.py --output-dir CONTROLS

The checker reads current Meson ownership, rejects missing/duplicate coverage,
inspects actual objects, and does not execute target binaries. The controls are
native tests and use the selected C++ compiler plus real GObject/GTK dependencies.
The evidence archive contains exact commands, object hashes, input closure and
control results; report.json seals the current repository sources.

This result covers GNU native Linux x86_64 app/worker objects without LTO. It does
not claim that CRT, shared dependencies or C resource code have no constructors:
the C-generated lebl resource initialization legitimately exists. OpenEXR's
separate optional plug-in and non-native platform toolchains are outside this
application C++ foundation proof. Future dynamic startup/TLS/resolver candidates
need explicit review; a detector finding is not automatically a demonstrated
pre-main GTK failure. Allocator audit04.019 and wider feature/platform gates remain
open.
