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

`c-entry-signatures.tsv` pairs 66 C++ definitions and header declarations
covering 65 names. It records both parameter lists and whether a C linkage
guard encloses the header declaration: 38 pairs have an identifiable guard,
and 28 need review. A missing guard does not prove a link failure in the old
build: some C++ files include the header inside a surrounding `extern "C"`
block. The new interface must make the intended linkage explicit.

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
Before 01.005 can be
completed, inspect each candidate, identify exported C functions and their
callers in changed C sources and public headers (including entry names missed
by the first-pass regex), trace indirect callbacks and
vfunc owners, and add any missing sites. Record the verified direction,
signature and ownership at each boundary. The parent WBS item remains open.
