# Original WBS 07.010: exception conversion at C and native vfunc boundaries

The common C11 entry matrix already exercises typed Painter Error,
std::runtime_error, std::bad_alloc and a non-standard integer exception through
result and void C++ boundaries. Each variant runs with and without GError output,
verifies safe failure, expected domain/code/message, one RAII destruction and
continued execution in the C caller. `test-c-api.c` is compiled as C11; its
adapters in `test-foundation.cpp` are C++14.

The native interface test previously covered std::runtime_error, fallback -1 and
successful recovery. The additional `/painter/hierarchy/interface-exception-policy`
regression strengthens that real vfunc path with the same four exception kinds,
with and without GError. The fixture's C `painter_readable_read` dispatches its
registered `iface->read`, an exact-signature C-linkage C++ trampoline. Injection
is a fixture-only Impl field, independent of public properties. Production code
and API are unchanged; no production defect was demonstrated.

Every failure enters the trampoline once, unwinds a local RAII guard once,
returns -1, preserves the model and owner refcount, and permits a subsequent
successful interface read. Typed Error retains its CLOSED code and message;
standard and allocation exceptions map to EXCEPTION with a nonempty message;
unknown exceptions use the fixed unknown-exception diagnostic. Error output is
caller-owned and cleared. NULL GError output never turns failure into success.
The existing child-value-13 regression and hierarchy behavior are preserved.

`native.json`, `sanitizers.json` and `meson.json` each contain 69 passing cases and
25 exact current source seals. The first new test compilation caught a conditional
expression passed unparenthesized to the GLib assertion macro; a local expected
code fixes that test-only issue before these measurements. Run the standalone
`tools/test_painter_foundation.py --build-dir DIR --report FILE`, repeat with
`--sanitize`, and run the existing Meson `painter-foundation` target.

The one source duty `legacy-bd2d7eb004105ee29db2` maps hunk `01.002/000045`,
old app/base/glib-cxx-impl.hpp blob
`a2ad35e37a3e7a481698c1f8b78b71adb385f7d3`. Its full fixed source is already
preserved at inherited-properties/legacy-source/glib-cxx-impl.hpp. The old
property adapters catch then exit at lines185–243; Binder invokes an Impl
without containment at453–456. Modern explicit C trampolines use common boundary
conversion and declared fallback values. This shared acceptance does not imply
all legacy Binder/callback call sites have completed their feature migration.

Historical native property failure tests in property-boundary/evidence.tar.gz
remain useful supporting history, but have subsequent source changes and are not
promoted to current passing evidence here. Full callback enumeration/containment
remains38.004/all-vfunc-exception-containment; property-specific behavior retains
its own acceptance. No LSan, system-GLib instrumentation, process-aborting GLib
allocation recovery or cross-platform result is claimed.
