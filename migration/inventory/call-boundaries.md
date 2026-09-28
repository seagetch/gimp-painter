# Legacy C/C++ call-boundary audit (01.005, in progress)

`cpp-call-boundary-candidates.tsv` is a reproducible **candidate** list from
all 71 `.cpp` and `.hpp` files under `app` at legacy commit
`afa43fae3e920210146abed514f136fd49f671b5`. Run
`python3 tools/inventory_call_boundaries.py` to regenerate it. The first pass
contains 658 source-line candidates: 69 `extern_c`, 28 C entry declaration
candidates, 149 C++ `gimp_*` definition candidates, 34 function-pointer sites,
63 direct vfunc assignments, 92 class-binding macro sites, 94 GLib callback
registrations, 22 signal-wrapper connections, and 107 delegator sites. The
definition scan handles return types on a preceding line. Comments are masked
before matching, and each row retains its legacy path and line number.
`boundary_statement` retains up to 12 source lines (bounded at 1,200
characters), so multiline signal registrations expose both the target and
the callback. For example, the clone layer `update` connection identifies
`GLib::CloneLayer::on_source_update` at `app/core/gimpclonelayer.cpp:284`.

The companion `c-call-reference-candidates.tsv` searches the legacy `app`
tree for 147 distinct C entry names from this pass. It found 130
references in `.c` and `.h` files. These include header declarations, type
macros and actual C call sites; the `kind` column distinguishes source files
from headers but does not yet classify each use. Both lists retain `REVIEW`
status until the C declaration and C++ definition signatures are compared.
`c-entry-coverage.tsv` places each of the 147 names beside its C++ definition,
header references and C source references. Of these, 65 have a header site
and 32 have a C source site; a name can occur in both groups.

`c-reference-review.tsv` classifies each of the 130 first-pass C/header
references against its legacy source line. Run
`python3 tools/audit_c_references.py` to regenerate it. The 80 header sites
contain 65 declarations, 11 type macro references, three C++ template
references and one commented-out declaration. The 50 C source sites contain
41 direct calls, six function-pointer registrations, one C definition and two
commented-out calls. Of the direct calls, 39 target C++ entry names; the
other two target the separate C implementation of the popup button helper.
This classification covers the existing candidate list, not all C uses in
the legacy tree. Caller argument types and owners still need inspection.

`c-entry-signatures.tsv` pairs 66 C++ definitions and header declarations
covering 65 names. It records both parameter lists and whether a C linkage
guard encloses the header declaration: 38 pairs have an identifiable guard,
and 28 were flagged for linkage review. Run
`python3 tools/audit_c_linkage_review.py` to verify the 28 source anchors and
regenerate `c-linkage-review.tsv`. Of those pairs, 25 use a matching header
included inside `extern "C"` in the C++ source; one defines
`gimp_mypaint_brush_button_with_popup` inside an explicit `extern "C"` block
at `app/tools/gimpmypaintbrushoptions-gui.cpp:322` (the implementation does
not include its own header). The remaining two pairs are a separate C++
overload and its template wrapper. The table lists candidate references in
C source files, including C definitions where applicable. This resolves the
legacy linkage classification, but does not yet audit every actual caller or
make the headers safe for arbitrary C++ includes. The port must give intended
C declarations explicit linkage guards.

`c-parameter-review.tsv` compares the parameter spelling of the 64 pairs
whose definition is in C++ and whose header declares that same entry. Run
`python3 tools/audit_c_parameters.py` to regenerate it. After normalizing
spaces around `*` and empty parameter lists, 61 parameter lists match, two
differ only in the parameter name, and
`gimp_perspective_guide_new` differs in `guint` versus `guint32` as well as
the name. The latter needs a deliberate public type choice and target ABI
check in the port. This comparison does not validate return types, typedef
equivalence, pointer ownership or runtime callers.

The `gimp_tool_options_button_with_popup` pairs are deliberately left for
review despite sharing a name. The legacy C function is defined in
`app/tools/gimptooloptions-gui.c:201`; the C++ helper in
`app/tools/gimptooloptions-gui-cxx.cpp:45` takes a different argument list
and has a separate C++ declaration in `gimptooloptions-gui-cxx.hpp:31`.
Their linkage and call sites must remain distinct in the port.

`bridge-boundary-findings.tsv` records nine inspected mechanisms in the old
`Delegator` and `NewGClass` bridges. It identifies the actual C-to-C++ callback
entry points, destruction paths and registration calls. In particular,
`Delegator::callback` invokes a C++ function with its `catch` block compiled
out under `#if 0`, and `NewGClass::Binder::callback` invokes an `Impl` method
without an exception boundary. Their replacement must contain exceptions at
the C ABI and preserve closure, GObject and private `Impl` lifetimes.

The `REVIEW` status is intentional. Pattern matches can include declarations
or pointer invocations that are not actual ABI boundaries. Conversely, a
more complex declaration or a C caller may not match. The signature list is
an audit aid, not a declaration that same-named functions have the same ABI.

`extern-c-block-review.tsv` gives the opening and matching closing line for
each of the 69 `extern "C"` scopes found in the C++ candidate list. Run
`python3 tools/audit_extern_c_blocks.py` to regenerate it. Of these scopes,
55 include C headers without a named `gimp_*` definition match, nine contain
named definitions without includes, three contain both, and two hold local
GObject type declarations and macros. The definition count is a pattern
match for triage, not an exported-symbol count; macro-generated entries and
the signatures of each callback need separate review.

`function-pointer-review.tsv` classifies all 34 first-pass function-pointer
syntax candidates: 13 declarations or function-pointer parameters and 21
indirect calls. Run `python3 tools/audit_function_pointers.py` to verify the
legacy source lines. This is a syntax classification; the pointed-to function,
its C ABI, lifetime and callback ownership remain to be traced.

`vfunc-assignment-review.tsv` maps the 63 direct class-field assignment
candidates to receiver, slot and value. Run
`python3 -B tools/audit_vfunc_assignments.py` to regenerate it. Twenty-three
assignments are metadata (labels, icons or flags), while 40 assign callbacks:
36 target a definition in the same C++ file, one targets the separate
MyPaint brush save source, and three target the generic GObject bridge.
`GClassWrapper::set_property` and `get_property` catch all exceptions and
call `exit(1)` in the legacy bridge. The GIMP 3 trampoline must define a
safe failure path without terminating the process. This table locates the
old targets; target signatures and ownership still require comparison with
the actual GIMP 3 class and interface slots.

Before 01.005 can be
completed, inspect each candidate, identify exported C functions and their
callers in changed C sources and public headers (including entry names missed
by the first-pass regex), trace indirect callbacks and
vfunc owners, and add any missing sites. Record the verified direction,
signature and ownership at each boundary. The parent WBS item remains open.
