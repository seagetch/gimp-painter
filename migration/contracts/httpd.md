# Optional Painter HTTP/REST contract

Pinned source: `afa43fae3e920210146abed514f136fd49f671b5` in the legacy checkout.
Source-family WBS:31.001–31.010 and31.008/router-lifetime. This document does not
mark behavioral gates passed; executable evidence is tracked separately.

## Complete registered endpoint/method ledger

| Registered path | Implemented legacy method and input | Legacy result/effect | GIMP3 contract |
|---|---|---|---|
| `/<name>` | Direct handler, any method | Intended `Hello, name` diagnostic; actual active `Soup::Router` only parses `{name}`, so the angle-token rule was a literal and name was unset | Corrected GET one-segment diagnostic; explicit method/status rather than reproducing unset-name behavior |
| `/api/v1/pdb/` | GET | Swagger2 document of all queried procedures, wildcard CORS | Swagger2 paths, current native parameter types, relative basePath, no wildcard CORS |
| `/api/v1/pdb/{name}` | GET | One procedure's Swagger path item,404 if absent | Same path-item shape; private native procedures excluded |
| `/api/v1/pdb/{name}` | POST JSON `{context,arguments}` | Synchronous PDB call, defaults for missing arguments, forced noninteractive run-mode, context IDs plus named `values` | Same envelope; current typed GimpValueArray/GObject arguments, precise validation, native error/status mapping |
| `/api/v1/images/**` | GET default or final `#info` | Root ID→display-name map; image name/type/boundary and child ID→name map; layer type/mode/opacity/boundary/visible/alpha, group child names | Preserved hierarchy and layer fields, including distinct clone/filter/group; additive IDs/child-ids/reference/procedure metadata |
| `/api/v1/images/**/#data` | GET | PNG at native image/layer dimensions via viewable rendering | Same viewable PNG route, bounded dimensions |
| `/api/v1/images/**/#preview` | GET | JPEG fitted within128px, aspect preserved | Same; minimum one-pixel thin dimension fixes zero-size old edge case |
| `/api/v1/images/**` | PUT default or `#info`, JSON | Root creates image from boundary/color-mode. Item creates normal layer before item in its parent. Old parser read but ignored name/type/offset/visible/alpha | Root image creation and sibling insertion kept. Requested fields now applied; group/clone/filter creation uses actual native types rather than silently creating normal layer |
| `/api/v1/images/**/#data` | PUT raw image or multipart containing image | Decode pixbuf, create image or sibling layer;201 `{result,image,layer,drawable}` | Same envelope and insertion; malformed/oversized data is explicit error, multipart first image part supported |
| `/api/v1/navigation` (GUI only) | POST JSON `{context,arguments:{message,webhook_uri,message_id}}` | Show nonmodal blue guide bar in image window; OK invokes callback once, hides bar; optionally POST `{context}` to webhook. `message_id` read but never transmitted | Nonmodal native dialog keeps canvas editing available;202 acknowledgment with context/message_id; OK alone completes; cancellation, same-window replacement, display/service teardown abandon completion; same webhook body and initial context identities |

Legacy GET/PUT/DELETE navigation, PUT/DELETE PDB, POST/DELETE image and PUT
preview were empty handlers. They now return405. POST PDB collection was a
missing-name failure; it remains404. Unknown routes/operations and expired IDs
return404. Bad JSON/types/ranges return400, unavailable GUI503, body limits413,
unsupported image MIME415. GIMP procedure calling errors map400, cancellations409,
and execution errors500. `error` is always a JSON string on request failure.

`#info`, `#data`, `#preview` are final path components, not URI fragments. Clients
must encode `#` as `%23` on the wire. Numeric path components are complete decimal
IDs; nonnumeric components are exact display/item names within the immediately
preceding container. The old partial-number parser (`123abc`→123) is intentionally
rejected as an ID. Duplicate-name lookup remains last matching direct child.

## Current PDB types and v1 enum separation

Scalar signed/unsigned integers are range checked, booleans remain booleans,
floating values must be finite, strings retain null, native enum nicknames and
integers are distinguished, and flags remain numeric. Image/item/drawable/layer/
channel/path/display/resource/unit references become checked native GObjects.
Context defaults to the user image and first selected drawable; supplied context
IDs override it, and mismatched image/object ownership is rejected. Native drawable
arrays default to the selected drawable. Explicit object arrays hold temporary
strong argument leases during execution (the native boxed array itself is weak).
Integer/double/string/byte arrays preserve ordering. Files use path/URI strings,
colors use four RGBA numbers, and parasites use name/flags/base64-data objects.
Native resource and drawable-filter IDs, export-option capability objects and
registered Babl encoding strings are also supported.
Unsupported registered types return an explicit501 rather than silently losing
arguments or results. Return values use names, or decimal positions when duplicate
names require the same legacy fallback. Run-mode always remains noninteractive.

Layer modes are an important exception to direct current numeric enum use:
`/api/v1` numeric0–29 are always the pinned Painter ordinal set, translated through
the existing shared `gimp_painter_layer_mode_from_legacy()` mapping. In particular,
23–29 mean Painter erase/replace/anti-erase/src-in/dst-in/src-out/dst-out. Modern
colliding modes must be named by their current nickname (e.g. `overlay`, `normal`).
The old `*-mode` nicknames keep legacy meaning. PDB returns a legacy number when
representable, otherwise a modern nickname. The implementation never relies on
accidental app/libgimp numeric equality at this protocol boundary.

## Optional build and activation

Build with `-Dpainter-http=enabled`, requiring libsoup3>=3.2. The default is
`disabled`: no app HTTP sources/libraries or libsoup dependency are linked.
json-glib is already a core dependency. The service does not participate in
painting, saving, preview, or ordinary canvas/tool UI.

Starting an enabled executable still opens no socket by default. Explicit runtime
activation requires `GIMP_PAINTER_HTTP_ENABLE=1` and
`GIMP_PAINTER_HTTP_TOKEN=<32..256 random URL-safe characters>`. Optional port is
`GIMP_PAINTER_HTTP_PORT` (default8920). Keep the token private; do not put it in a
URL or public script. An authenticated client can invoke PDB operations with the
application user's privileges, including file/plug-in operations. Enable only for
trusted local integration sessions and stop/restart the app to revoke it.

Security changes are deliberate and visible:

- IPv4 numeric loopback only; no all-interface listener or host setting
- Exact `Authorization: Bearer …` required for every route, including discovery
- Reject every Origin header and nonnumeric/nonloopback Host; no wildcard CORS
- PUT/POST require bounded Content-Length; no chunked input;16MiB raw body and
  1MiB JSON,64 queued requests,64-megapixel/32768-dimension image limits
- No response caching or MIME sniffing; no token/content logging
- Webhook disabled unless one exact `http://127.0.0.1:PORT` origin was explicitly
  configured using `GIMP_PAINTER_HTTP_WEBHOOK_ORIGIN`; URI credentials/fragments,
  other origins, DNS hostnames and redirects are rejected. Callback I/O is async,
  cancellable and has a10-second timeout. There is no remote webhook access

Tests use ephemeral port0 and close listeners before exit. They never activate a
persistent service, alter host firewall/network permissions, or accept remote
access. External integrations must add bearer authentication and use numeric
loopback URLs; this is an intentional protocol security upgrade.

## Ownership/main-context design

Gimp owns one ordinary GObject service reference. Its C++ implementation is one
typed slot in that service object's existing common BindingStore. No Impl qdata,
NewGClass, Interface::cast, or second implementation ownership mechanism exists.
The service retains a weak application reference. Gimp's disposal and accepted
application exit clear the service before resources/UI disappear.

Soup callbacks authenticate and copy request bytes only. Each accepted request is
paused and transferred to an owned Source on the captured application main
context. Only that source resolves current image/PDB/context IDs and touches UI.
Requests do not capture borrowed image pointers across the transfer. Closing the
service destroys queued sources, completes/cancels paused messages, aborts hooks,
destroys its per-window guides and destroys its router. Reentrant stop during PDB execution
cannot publish a late response; a local router lease keeps the executing resource
alive until the call returns. Rule and Resource base destructors are virtual and
unique_ptr-owned, fixing both old nonvirtual-destructor families and shared raw
factory leaks. Guide closure payloads are destroyed on all exits, including when
no webhook exists.

## Caller audit

`migration/inventory/httpd-callers.txt` is a tracked-source grep at the exact
pinned commit. `httpd-network-callers.txt` separately searches generic client
mechanisms, not merely literal `/api/v1` strings. Registration is only through
`app/gimp-features.cpp` factories (GUI/console selection); the GUI navigation
completion is the only outbound caller. OpenAPI2/3 URL strings are documentation
publishers; GET selects OAS2 in the actual implementation. The image-window
navigation helper is only called by the HTTP guide resource. Numeric brush/dynamics
and SVG `8920` matches are false positives. The independently configured Script-Fu
TCP server/test is a separate protocol and is not an HTTP client. No image save,
brush asset, painting tool, canvas tile or compact toolbar relies on this service.
External/untracked clients cannot be enumerated from this checkout; retained API
shape plus documented security/type adaptation is their compatibility boundary.

The supplemental dependency lock is replayable using the existing isolated
extractor after the main dependency lock:
`python3 tools/prepare-linux-build-deps.py --directory <shared-deps> --lock migration/baseline/httpd-debian13-package-lock.json`.
It uses signed snapshot metadata and validates every archive SHA256 before local
extraction. No host installation or protected host APT configuration is required.

### Native creation ownership

A successfully created document is not destroyed merely because the optional
service stops. GUI creation transfers the initial reference to its first display,
as native `gimp-display-new` does. Headless creation transfers the initial reference
to the native image-delete/display-new protocol. This is essential: native
`gimp-image-delete` consumes that persistent creation reference. Keeping the same
reference in a service-owned ObjectRef would create a dangling double owner after
PDB deletion. Failed creation remains locally RAII-owned. A regression explicitly
creates headlessly, deletes through the PDB route, then destroys the router.

### Reviewed callback bounds

The outbound webhook session has its proxy resolver disabled, so system HTTP proxy
configuration cannot route loopback context to a remote proxy. Each request has a
separate cancellable and absolute10-second main-context deadline. Response headers
finish the callback; its response stream is immediately closed/discarded rather
than buffering an unbounded body. No redirects are followed. The ordinary Soup
socket timeout is additional and is not mistaken for the absolute deadline.


## Executable verification checkpoint (2026-10-02)

- Enabled normal:17 native protocol/type/lifetime cases and6 real GTK navigation
  cases, including multi-window scopes and shutdown during an in-flight webhook
- Same17+6 cases pass ASan/UBSan/vptr and float-cast-overflow instrumentation:
  19 HTTP/native lifecycle units instrumented,38 production C++ compatibility
  units compiled with RTTI only; GTK/Soup/other dependencies uninstrumented,
  leak detection disabled
- Disabled build:1459 initial selected compile/link steps; all HTTP sources and
  HAVE_PAINTER_HTTP absent; GUI and console executables contain no HTTP start
  symbol or Soup DT_NEEDED. An isolated pkg-config wrapper rejects Soup queries
- Disabled runtime:10 painting/surface,20 XCF save/read roundtrip and19 native
  Canvas UI cases pass. Generated image/dockable menu assets are required by the
  fresh GUI harness and included in the reproducible build command

Run `migration/tests/run_http_checks.sh BUILD normal` or `asan` after sourcing the
build environment. The focused builder is `build_httpd_sanitizers.py`; ASan replay
verifies exact source archive and executable hashes, plus current HTTP source/test
identity. Separate migration work may advance an unrelated RTTI-only unit after
the snapshot; the archive records the tested version. Three precision-surface
inputs were recovered byte-for-byte from Git into this archive; current source was
never overwritten. This is a focused snapshot, not an aggregate proof of every
later source-family change.

For optionality, run `run_http_disabled.sh BUILD build` then its `run` mode on a
native GTK display. `verify_http_disabled.py` checks both executables and the
compile database. Test clients disable proxy resolution and use per-process
GSETTINGS_BACKEND=memory. An initial protected-home dconf failure was preserved;
no protected path or system permissions were changed. Known standalone harness
warnings about missing source-tree icons/toolrc entries and its non-searchable
writable test data folder remain visible; all reported exit statuses are zero.

Remaining integration gates: aggregate current-tree verification after other
migration families finish; other supported OS/toolchain builds; and testing any
external clients not provided in the pinned repository against the documented
activation/authentication/Origin/webhook policy changes. No actual persistent
runtime service has been activated during this work.
