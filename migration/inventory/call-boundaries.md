# Legacy C/C++ call-boundary audit (01.005, in progress)

`cpp-call-boundary-candidates.tsv` is a reproducible **candidate** list from
all 71 `.cpp` and `.hpp` files under `app` at legacy commit
`afa43fae3e920210146abed514f136fd49f671b5`. Run
`python3 tools/inventory_call_boundaries.py` to regenerate it. The first pass
contains 288 source-line candidates: 69 `extern_c`, 28 C entry candidates,
34 function-pointer sites, 63 class/interface vfunc assignments, and 94
callback registrations. Comments are masked before matching, and each row
retains its legacy path and line number.

The `REVIEW` status is intentional. Pattern matches can include declarations
or pointer invocations that are not actual ABI boundaries. Conversely, a
multiline declaration or a C caller may not match. Before 01.005 can be
completed, inspect each candidate, identify exported C functions and their
callers in changed C sources and public headers, trace indirect callbacks and
vfunc owners, and add any missing sites. Record the verified direction,
signature and ownership at each boundary. The parent WBS item remains open.
