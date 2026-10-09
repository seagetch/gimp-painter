# Existing 05.013/class-init-error: native class initialization

MyPaint options now pass their generated names directly to GParamSpec, which
copies and canonicalizes underscores. The redundant std::string conversion was
the only direct allocating C++ preparation in the current 31 types / 38 class and
interface initializers. Removing it prevents a C++ exception from unwinding
through native GType initialization. Property names, types, ranges, defaults,
flags, callbacks and the serialized settings ABI remain unchanged.

The native rule is documented by [GObject ParamSpec](https://docs.gtk.org/gobject/class.ParamSpec.html).
This code does not use G_PARAM_STATIC_NAME. The regression checks actual GLib
2.84.4 results; it does not infer the installed library's behavior solely from
the documentation for another version.

## Reproduced failure and current acceptance

The fixture compiles the actual baseline/current options translation unit and
links the native GIMP application's real dependencies. It replaces C++ allocation
only in the test executable, verifies interception using an allocating string
control, and arms its thread-local failure gate only around the first options
class reference. GEGL initialization and parent-class prewarming happen first;
the options class is still uninitialized when the gate is armed.

All 60 bounded subprocesses match their expected outcomes:

- The baseline makes 22 C++ allocations in the name conversions. Each of the 22
  failure positions escapes as bad_alloc, both normally and under ASan/UBSan
- At the first/middle/last failure, 2/31/49 real GParamSpecs are prepared but none
  of the 55 own properties is installed. These are floating preparation objects,
  not a partially installed current property table
- Retrying the first baseline failure with injection disabled returns a pointer
  with the correct GType, but public class lookup remains NULL and only the 71
  inherited properties exist. The same two prepared specs remain. The type is
  unfinished; it is not a different GType or a recovered registration
- Fixed initialization makes zero intercepted C++ allocations and completes even
  with the first-allocation failure gate armed. The independent positive control
  still triggers, so zero is not an unverified failure-injection assumption
- All 126 properties are checked: 55 own and 71 inherited. Own owner/ID, canonical
  names and underscore lookup, types, flags, defaults and numeric ranges match.
  Native class/vfunc/interface/signal identity, wrong-instance rejection,
  16 repeated class references and 16 valid constructions are verified

The existing application-linked options suite was independently rebuilt against
the fixed production library and passes 10/10. The focused normal/sanitizer runs
report no timeout or ASan/UBSan diagnostic. Existing configured-data-folder
warnings at successful application teardown are retained. LSan remains disabled
because of the established ptrace restriction; vptr checks are omitted with the
native no-RTTI configuration. Linked dependency archives are not sanitizer builds.

## Legacy pointer exception and partial registration

The pinned legacy glib-cxx-impl.hpp has Git blob
`a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`. Its WithClass::init stores a mutable
class pointer and throws an allocated InvalidClass pointer on a different class.
Its install_property publishes a native property before copying the C++
Property/std::function entry. That old post-install failure is distinct from the
current baseline's pre-install name-preparation failure.

Current production has no WithClass/GClassWrapper/NewGClass/InvalidClass or
with_class(gpointer) entry. Native G_DEFINE_TYPE registration supplies the class
pointer, while typed instance APIs reject a wrong native object without modifying
class/spec state. This removes the mutable-wrapper mismatch interface; it is not
an execution or recreation of that removed API. The exact legacy source and its
20 broader header obligations are preserved in the evidence. None of those
separately assigned foundation duties is silently completed here.

## Scope of the correction

There is no recoverable C++ class-preparation operation left in the reviewed
initializers. Other bodies assign functions/scalars or call native registration;
captureless interface lambdas execute their bodies later and remain callback
scope. This fix eliminates the measured exception path. It does not introduce or
claim rollback of an already registered GType, retry of a broken class, recovery
from GLib fatal allocation failure, or universal exception containment in future
class code. Any future fallible class preparation must satisfy the preflight,
containment and terminal-failure policy in cpp-error-boundary.md.

The child closes on current source removal/type identity, measured incomplete
baseline registration/retry state and complete fixed registration. The separate
legacy property-exit and all-vfunc failure gates remain open. Earlier immutable
source-sealed checkpoints become historical where this legitimate source change
differs; their stored hashes are not rewritten. The new checker verifies that all
current native parent/slot assignments still equal the 05.011 contract.

Run `python3 tools/check_painter_class_initialization.py` for source/evidence and
type/slot consistency. The evidence archive contains the exact native fixture,
commands, source snapshots, compact logs and reproduction instructions; it omits
binaries, object files and generated runtime profiles.
