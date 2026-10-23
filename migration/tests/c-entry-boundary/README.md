# Common C-entry exception conversion — original 06.013

The existing noexcept boundary/boundary_void helpers catch typed Painter errors,
standard exceptions and unknown C++ exceptions. They supply the declared native
fallback and translate to GError when requested. The actual common
`gimp_painter_binding_close` C entry encloses its validation and store close in
that boundary. No production implementation or test case is added here.

The existing test-c-api.c is compiled by the C compiler. Its actual C callers
exercise C→C++→C values and exception return paths. The result and void boundary
policies each cover Painter Error, std::runtime_error, std::bad_alloc and an
unknown non-standard exception, with a GError output and with NULL. The C code
continues after every call, checks fallback/error domain and code, validates
messages where stable, frees GError, and observes the C++ cleanup destructor
exactly once. The common C close entry also checks invalid owner conversion,
empty-owner no-creation and normal close behavior.

All compiled sources/headers and both binaries are byte-equal to the preceding
06.012 full validation (51 native and 51 rebuilt ASan/UBSan cases). Against
those verified binaries, this task freshly runs the five interop cases plus the
common-close case: 6 native and 6 sanitizer cases pass, stderr empty. The exact
C compile command is retained: it invokes cc on test-c-api.c, not C++
recompilation. No redundant test or fresh full-suite build is claimed.

The sanitizer build instruments the 11 common C/C++ units, not system GLib.
LSan remains unavailable under the verified ptrace limitation and vptr is
excluded for native no-RTTI compilation. The std::bad_alloc case is controlled
C++ exception injection; it does not claim survival of process-wide native
allocator exhaustion. GError outputs follow the ordinary GLib empty-output
precondition.

Work item `legacy-bc8c738def225a9ac706` maps pinned glib-cxx-impl.hpp hunk
`01.002/000045`. Old Binder::callback directly invokes the Impl member without
an exception boundary. Old property get/set catch exceptions but terminate via
exit(1). The modern common C boundary returns fallback/GError; GObject property
callbacks without an error return use the separately implemented diagnostic
policy documented in ../property-boundary/README.md. Native class initialization
and each feature adapter keep their own containment obligations. This closes the
common C-entry conversion criterion, not every future adapter. Exact pinned
source/Git blob/file and hunk digests are verified. Only this source row's
mutable execution fields become DONE.
