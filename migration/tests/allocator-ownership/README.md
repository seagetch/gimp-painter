# Original 04.019: allocator ownership across the C/C++ boundary

The audit maps the current C++ application/worker interfaces to their actual
native producers and destructors. No current new/delete versus GLib allocation
family mismatch was found. It did find acquisition-to-owner gaps: a C++
allocation failure could skip the correct destructor. These gaps are fixed
without changing the shared BindingStore architecture or native C ABI.

## Scope and allocation families

The source inventory covers all 78 production application/worker C++ translation
units in the HTTP-enabled configuration (73 without HTTP), their ownership
headers, and the native C producers/consumers named below. The separate optional
OpenEXR wrapper is also inspected for allocator pairing; it is not runtime tested
by these builds. This is a boundary audit, not a claim that every upstream C
allocation or every third-party library was exhaustively fault-injected.

| Boundary | Producer and matching release | Source anchors |
| --- | --- | --- |
| Shared implementation store and typed slots | C++ new/unique_ptr/default delete; native owner GObject is separately ref/unref managed | app/painter/binding-store.cpp; app/painter/binding-store.hpp; app/painter/gimp-painter-binding.cpp |
| Object/weak references | GObject ref/sink/unref; C++ WeakState new/delete with g_weak_ref_clear | app/painter/object-ref.hpp |
| Signals and main-context sources | C++ callback payloads use typed delete in GClosureNotify/GDestroyNotify; GSource uses destroy/unref | app/painter/connection.hpp; app/painter/source.hpp; app/painter/fair-dispatcher.hpp |
| Strings and errors | g_strdup/g_malloc/native GLib text producers use g_free; GError uses g_error_free before a potentially throwing std::string copy | app/painter/resources.hpp; app/painter/boundary.hpp; app/paint/painter-native-stroking.hpp |
| GValue and native arrays | Stack GValue is unset, never freed; GArray/GPtrArray/GBytes use native unref and declared element notifiers | app/painter/resources.hpp; app/painter/bytes.hpp; libgimpbase/gimpvaluearray.c |
| Numeric boxed arrays | GimpArray outer g_slice_new0/g_slice_free; mutable g_memdup2/g_try_malloc payload g_free; vector inputs use copying setters | libgimpbase/gimpparamspecs.c; app/core/gimpfilterlayer-arguments.hpp; app/xcf/painter-xcf-arguments.cpp; app/httpd/httpd-pdb.cpp |
| String vectors | GLib pointer table and GLib strings enter G_TYPE_STRV/g_strfreev; partial construction has the same deleter | app/core/gimpfilterlayer-arguments.hpp; app/xcf/painter-xcf-arguments.cpp; app/httpd/httpd-pdb.cpp |
| CoreObjectArray | g_malloc0 pointer-table copy/g_free; the boxed table does not own its objects, so separate ObjectRefs retain elements | libgimpbase/gimpparamspecs.c; app/core/gimpfilterprocedure-arguments.cpp; app/httpd/httpd-pdb.cpp |
| Color/parasite/Babl boxed values | Colors copied/ref-released with g_free table; parasite dedicated native free; Babl identity/no-op boxed release | libgimpbase/gimpparamspecs.c; libgimpbase/gimpparasite.c; libgimpcolor/gimpcolor.c; app/httpd/httpd-pdb.cpp |
| Filter public snapshots | C++ new is returned as opaque C handle; only gimp_filter_arguments_snapshot_free performs delete | app/core/gimpfilterlayer.cpp; app/dialogs/painter-layer-dialog.cpp; app/xcf/painter-xcf-preserve.cpp |
| Clone public references | g_new0 outer and g_strdup fields; dedicated free releases fields, native source ref and outer with their own families | app/core/gimpclonelayer.cpp; app/core/gimpclonelayer-handle.hpp; app/xcf/painter-xcf-preserve.cpp |
| Provenance public transport | g_new0 outer/g_free; fields are g_free strings and ref-owned GBytes/GVariant/GPtrArray; stack transport inputs are borrowed | app/core/gimp-painter-provenance.cpp; app/core/gimpimage-duplicate.c; app/xcf/painter-xcf-transport.cpp |
| Filter capture/process/raster/spool | std::vector/string/shared_ptr retain C++ storage; GEGL/GIO/JSON objects use native release; GEGL pixel calls borrow data synchronously | app/core/gimpfiltercontext.cpp; app/core/gimpfilterprocedure.cpp; app/painter/filter-process.cpp; app/painter/filter-raster.cpp; app/painter/filter-spool.cpp |
| XCF save state and mapped storage | XcfPainterSave new/dedicated delete; GBytes custom release deletes its C++ File owner, whose destructor unmaps/closes/unrefs native resources | app/xcf/painter-xcf-preserve.cpp; app/xcf/painter-xcf-storage.cpp; app/xcf/xcf.c |
| XCF records and imports | Native record arrays own GBytes refs; strings/STRV/numeric arrays use their boxed native release; vector/stack bytes are copied, not given to generic free | app/xcf/painter-xcf-load.cpp; app/xcf/painter-xcf-multipart.cpp; app/xcf/painter-xcf-arguments.cpp; app/xcf/painter-xcf-transport.cpp |
| Deferred-save completion | C++ Request invokes supplied destroy callback exactly once; C completion g_new0 is released by its g_free callback | app/dialogs/file-save-deferred.cpp; app/dialogs/file-save-dialog.c; app/actions/file-commands.c |
| Resource and profile API text | JSON/path/checksum/icon bytes/returned names are GLib allocations or native objects with String/ObjectRef/native release; APIs do not transfer std::string backing storage | app/core/gimppaintermybrush.cpp; app/core/gimppainterprofile.cpp; app/paint/painter-mypaint/resource.cpp |
| Paint masks/patterns/pixels | Native brush/pattern owners retain borrowed views; vectors retain copied pixels; saved native undo description uses g_strdup/g_free | app/paint/painter-mypaint-surface/gimp-resources.cpp; app/paint/painter-mypaint-surface/paint-core.cpp; app/paint/gimpfillbrush.cpp; app/paint/gimppaintersmudge.cpp; app/paint/gimppainterpaper.cpp |
| Guide, tool and widget payloads | Typed C++ callback destroy functions delete new payloads; native widgets/TempBuf/pixbuf/Cairo objects use their own lifetime APIs | app/core/gimpperspectiveguide.cpp; app/core/gimpperspectiveguideundo.cpp; app/tools/gimppaintermybrushtool.cpp; app/widgets/gimppaintermybrusheditor.cpp |
| Native C++-compiled upstream kernels | g_new/g_free buffers and g_slice_new/g_slice_free task objects; TempBuf references use native unref, not delete | app/core/gimpbrush-mipmap.cc; app/core/gimpbrush-transform.cc; app/core/gimp-parallel.cc; app/core/gimppickable-contiguous-region.cc; app/gegl/gimp-gegl-loops.cc |
| JSON/HTTP response ownership | JsonParser/JsonNode native release, serialized and encoded bytes g_free, member GList nodes g_list_free while names remain borrowed | app/httpd/httpd-resource.cpp; app/httpd/httpd-images.cpp; app/httpd/httpd-pdb.cpp |
| HTTP async message/URI | SoupMessage ref/unref; GUri/g_uri_unref; new callback payload consumed by typed unique_ptr; Soup copies std::string response bytes | app/httpd/httpd.cpp; app/httpd/httpd-navigation.cpp |
| Borrowed image results | Selected-drawables list is a copy and its nodes are freed; selected-layers and cached preview pixbuf are borrowed and not released by the HTTP consumer | app/core/gimpimage.c; app/core/gimpviewable.c; app/httpd/httpd-images.cpp; app/httpd/httpd-resource.cpp |
| Platform executable path | macOS realpath(NULL) uses std::free; separately copied GLib text uses g_free | app/core/gimpfilterpaths.cpp |
| Optional OpenEXR C/C++ wrapper | EXRLoader new/dedicated delete; g_strdup/g_memdup2 metadata handed to C caller is g_free-owned | plug-ins/file-exr/openexr-wrapper.cc; plug-ins/file-exr/file-exr.c |

## Corrections and measured failures

Router rules are now owned before vector growth. Parsing immediately owns its
JsonParser and GError; JSON and image encoding results are guarded before response
strings allocate. PDB member-list nodes, HTTP URI/errors and webhook message/
callback construction use matching owners before throwing work. Empty JSON is
rejected without copying a NULL JsonNode.

FairDispatcher constructs its allocating deque before retaining raw native
context/thread references. The shared take_error_message helper consumes and
nulls an owned GError before copying its message. Callers preserve typed error
domain/code before transfer. Existing already-owned errors are not adopted twice.

Identical native fixtures reproduce the old failures and exercise the fixes:

- HTTP: 29 processes, 17 injected C++ failures; six baseline leak cases become
  zero in both normal and ASan/UBSan runs. Six separate native-new/delete controls
  also pass. The parser, JSON serialization and GLib objects are real; only Router
  resource factories are bounded counting fixtures
- Dispatcher: 20 observations, 13 injected failures; six baseline cases retain
  context/thread refs, versus zero after the fix. Normal and ASan/UBSan sweeps
  agree; four separate native-new/delete lifecycle controls pass
- Error transfer: the baseline leaves one real GError alive on failed string
  allocation; the fix clears the pointer and frees exactly once. Fixed normal and
  sanitizer runs each pass 49 checks, with 33 native allocator control checks
- An intentional new/free mismatch control produces the expected ASan diagnostic;
  a baseline fixture reporting PASS means the expected baseline defect was proved

The same immediate-owner correction is used at the inspected encoding, PDB and
webhook boundaries. Those exact sites are source-audited and exercised by the
ordinary native HTTP suite; they are not falsely counted as individually
fault-injected by the focused Router/JSON fixture.

## Current validation

The foundation (40), dispatcher (8) and native HTTP (17) suites pass. The current
foundation also passes a separate ASan/UBSan build. Selected changed-boundary
regressions pass: MyPaint resource 8, Fill 36, Smudge 23, profile parser 9, paper 12,
XCF roundtrip 49, and the Filter process suite. Both existing direct C/C++ header
probe archives compile (357 default / 359 HTTP). These are targeted results, not
a claim of a fresh full application test-suite pass.

The real GTK brush Save As/Rename/Delete flow passes. A proposed normal Delete
entry leak was disproved: weak notification records finalization before the
response even in the retained implementation. The unnecessary prompt change was
reverted; no Delete leak fix is claimed.

The original native profile executable also passes all 9 cases after supplying
the legitimate generated tags XML absent from the restored installation prefix.
An identical diagnostic binary reproduces the missing-file failure and then
passes the installer case with that one prerequisite installed. Initial 6/9 and
isolated-diagnostic shutdown failures remain recorded; the latter's root cause is
unestablished and it is absent from the original full suite. This comparison does
not claim a separate full parent-commit build or a warning-free native startup.

LeakSanitizer is unavailable because its runtime reports a ptrace restriction.
ASan/UBSan runs use detect_leaks=0; leak conclusions come from explicit allocation
and real native-owner/ref counters. Fault-injection runs replace C++ new/delete
with counted malloc/free, so separate native-new/delete controls establish the
sanitizer's allocator-family check. RTTI-dependent vptr checks are excluded by the
native no-RTTI policy. No GLib abort-on-OOM recovery or all-program leak freedom is
claimed. Platform-specific pairing was inspected, but Windows/macOS execution,
tablets and the separate AT-SPI dependency issue remain open.

## Reproduction and checkpoint meaning

The evidence archive contains the focused test sources/scripts, exact commands,
source hashes and compact results. The ordinary native tests remain registered
in Meson. After unpacking the fixtures, their READMEs describe how to select the
native build/dependency environment; recorded absolute build paths identify this
run and must be adjusted for another checkout.

Run `python3 tools/check_painter_allocator_ownership.py` to validate this source
checkpoint, evidence members and result invariants. It verifies an audit record;
it is not a whole-program allocator theorem prover. Earlier source-sealed reports
retain their historical hashes and are not silently relabeled as current after
these legitimate source changes. There are zero canonical source obligations
assigned to 04.019; obligations assigned to other original WBS items remain open.
