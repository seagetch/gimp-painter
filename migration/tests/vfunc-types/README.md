# Exact native vfunc types — original 06.014

The current adapters implement the contract in cpp-foundation.md: exact native
slot signatures and ordinary function assignment. No generic Binder or second
bridge is introduced. The test hierarchy now statically compares the actual
PainterReadableInterface.read member type with its C-linkage C++ trampoline,
and dynamically checks that the installed interface pointer is that function.
The existing native C virtual dispatcher invokes it and verifies its result and
contained exception behavior.

The current source gate records 31 types in 27 production files, 39
class/interface initializers and 168 slot assignments across 26 of those files.
The remaining gimptooloptions.c defines GimpPainterDeviceOptions without an
override, so it has no vfunc assignment to compile in this gate. There are 164 named functions,
two null hooks and two captureless lambdas. Complete assignment expressions are
checked; casts and G_CALLBACK transport in a vfunc slot expression are rejected.
Six expression rejection tests also cover the old cast forms, signal transport,
captured lambdas and immediate/postfix lambda invocation. The actual 26
production sources and two real C fixture sources pass syntax
compilation using their configured native commands. C incompatible-pointer
warnings are errors, and permissive C++ compilation is prohibited.

Two positive native GObjectClass/PainterReadableInterface assignment controls
compile in C and C++14. Ten negative controls fail for the expected assignment
diagnostic: wrong return, owner-pointer type, arity, GValue constness and
interface return type, each in both languages. A separate declaration extracted
from the actual big-endian EndianInput implementation matches the native
GInputStreamClass.read_fn type. This declaration check does not execute a
big-endian runtime or claim that its conditionally disabled body was compiled.

The rebuilt native foundation and rebuilt common-component ASan/UBSan suites
each pass 51 cases, including real property/interface dispatch and lifecycle.
The new checks strengthen an existing case without adding duplicate runtime
cases. The component's 11 C/C++ units are instrumented, system libraries are
not. LSan remains unavailable under the verified ptrace limitation and vptr is
excluded for native no-RTTI compilation.

The historical original05.011 snapshot remains unchanged at 31 types,
38 initializers and 167 slots. The additional current slot is the independently
implemented GimpPerspectiveGuideUndo GInitableIface.init. The new report records
current source hashes and results; it does not rewrite old evidence digests.

Work item legacy-5fd71556619bd98df0f0 maps pinned glib-cxx-impl.hpp hunk
01.002/000045. Its Binder::bind overload accepts independent R2/G2 types and
reinterpret_casts them to the destination Ret/G signature. The wrong-return and
wrong-owner controls specifically prohibit that unchecked adaptation. Modern
C/C++ trampolines enter the shared BindingStore/boundary through native typed
functions. Exact legacy blob, whole-file and addition-hunk digests are verified.
Only this source row's mutable execution evidence becomes DONE.

This gate covers custom class/interface vfunc assignments, not all callbacks or
an arbitrary C++ alias/dataflow proof. Framework G_DEFINE registration casts and
signal G_CALLBACK transport are distinct native mechanisms. Three reviewed
non-vfunc GDestroyNotify conversions remain outside this gate: g_bytes_unref in
app/core/gimp-painter-provenance.cpp and app/xcf/painter-xcf-transport.cpp, and
g_uri_unref in app/httpd/httpd.cpp. Their pointer signatures differ from
void(gpointer); no runtime failure was reproduced here. They retain a separate
resource/callback adaptation follow-up under original06.021. Feature behavior,
all-platform execution and the remaining lifecycle gates are not closed by a
successful function-type check.

Run tools/check_painter_vfunc_types.py with the configured --build-dir, scratch
--work-dir and --report paths; the evidence retains exact commands, controls,
source hashes and outputs. Its source scanner accepts only the explicitly
supported native assignment forms; compiler results establish their actual
configured types.
