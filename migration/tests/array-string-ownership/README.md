# GLib array/string owners — original 06.021

ArrayRef already owns GArray references through explicit adopt/retain/release,
with copy/move temporary-and-swap and detach-before-reset. String is a move-only
unique owner with g_free; explicit string copies use g_strdup or std::string.
GArray element ownership remains the declared native clear function's contract.
This task accepts those implementations and the existing array-reassignment
child, and removes three incompatible destroy-function pointer casts.

Provenance and XCF transport now supply native gpointer callbacks that cast the
data pointer to GBytes* before g_bytes_unref. HTTP URI-list destruction similarly
uses gpointer to GUri* before g_uri_unref. The release functions, reference counts
and order are unchanged. This corrects callback type compatibility; no prior
runtime crash from these casts was reproduced. No new ownership mechanism is
introduced.

Current common tests verify null/adopt/retain/copy/move/self/empty/release paths,
shared GArray contents, displaced arrays, and exactly-once element finalization.
External producer ownership and explicit release are checked independently.
Empty arrays are not indexed. Copy/move/reset finalizers may reenter and install
a replacement array. String tests cover independent duplication, UTF-8 text,
move/self-move, replacement, empty/reset/release, and unwinding a GLib allocation.
String checks use the actual g_free deleter and sanitizer allocator interception;
they are not presented as a LeakSanitizer result or separate per-string counters.

Rebuilt normal and common-component ASan/UBSan suites each pass 59 cases.
The three changed production translation units rebuild, and fresh native
Provenance12, HTTP17 and XCF49 suites pass. Those production suites are normal
builds, not newly instrumented whole-application sanitizer runs. The XCF suite
emits its expected malformed/unknown-record diagnostics; HTTP/XCF retain the
existing synthetic-profile writable-data-folder shutdown warning. Logs are
preserved rather than described as warning-free. All 11 common C/C++ units are
instrumented; system libraries are not. LSan remains unavailable under the
verified ptrace limitation, and vptr is excluded for no-RTTI compilation.

## Source-specific ownership correspondence

- glib-cxx-utils.hpp: CString/MemoryHolder map to native g_free String ownership;
  Array/IArray to explicit GArray adopt/ref/unref. Old StringList uses
  g_strdupv/g_strfreev; current STRV construction frees partial vectors and
  transfers them to native G_TYPE_STRV. Old GLib::String owns GString and is a
  different type from current GimpPainter::String. Growable C++ text uses
  std::string; a new unused GString wrapper is not introduced
- Its existing array-assignment child: old assignment overwrites an owned
  reference and clears self-move. Current replacement first owns the incoming
  reference, publishes it, and then releases the old reference. Current tests
  establish that replacement contract, not a new legacy runtime failure oracle
- json-cxx-utils.hpp: Json/JsonFree use json_node_unref, set/append transfer node
  ownership, member returns a parent-owned borrow, and parse owns its parser and
  copies its borrowed root. JSON member lists free only the nodes; names stay
  borrowed. Serialized/encoded native text uses String before C++ allocation.
  Numeric output uses vector plus copying native setters, and STRV uses its
  matching boxed transfer. JSONPath/builder DSL and full resource semantics
  remain separate obligations
- scopeguard.hpp: typed native deleters and std::unique_ptr preserve each
  producer's release family. C++ resources pair new/delete; native references
  pair ref/unref. Nullable is a borrowed output helper, not an allocator owner
- selectcase-utils.hpp: copied matcher strings and referenced pattern arrays have
  explicit native or C++ owners. The old one_of return is not claimed to dangle:
  IArray's old factory adds a reference. Its transfer is implicit; current
  adopt/release makes transfer explicit. This ownership mapping does not claim
  complete regex/string/GType selection behavior

ownership-correspondence.json in the archive records exact current source paths
and hashes. All four pinned old files have verified Git blob, whole-file and
addition-hunk hashes. Only the four original06.021 source obligations and the
already-listed array-reassignment obligation become DONE. Historical04.019 fault
injection remains historical; some dependency headers have since gained helpers,
so that full snapshot is not relabeled a current pass. Future JSON/selection,
construction, caller and platform gates remain open.
