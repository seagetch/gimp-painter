# Bounded native Normalize owner route

This adds the literal `plug-in-normalize` route for nonlinear U8, alpha-bearing
RGB Filter owners with exactly three saved `G_TYPE_INT` context slots and no
extra parameters. The saved slots remain model data; native owner context is
captured by the existing owner path. RGB and RGBA lower inputs are covered.
Gray, indexed, no-alpha owners and other precisions are explicitly unsupported.

The unchanged authoritative CropRaster transport captures the selected rectangle
at start; native masks, components and alpha lock are merged late. Input extents,
vector/spill paths, cancellation, cache preservation and empty-selection no-merge
are checked. The concrete stats/LUT types are statically bounded to one KiB in
the logical memory reservation; this is not a process-RSS guarantee.

Nonempty all-transparent input and hidden RGB outside visible extrema are old
undefined domains. They are never executed by the old oracle. The new worker
reports a useful error without replacing cache, pixels, dirty state or history,
retains the definition, and permits a subsequent valid generation. Worker-start
count is one, not zero: rejection before publication is not rejection before
execution. Empty selection is separately the existing successful no-merge path.

## Evidence scope

`seal.json` identifies exact committed 55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94
source plus the two committed 7e63bf6ad0 point-shadow wrappers and three owned
Normalize provider/test files. Only private objects and private thin archives
were linked; shared clean sources and linked input bytes were checked unchanged.
This is **not** a latest-HEAD aggregate or a new full application build.
`baseline-source-sha256.json` pins the complete committed source, `sources.tar.gz`
contains the tested overlays and selected provider/header/fixture closure, and
`linked-before.json` pins actual linked object/library bytes.

Fourteen normal selections comprise eleven new Normalize cases and unchanged
Red Eye, Colorify and Exchange basic regressions. Eleven Normalize cases are
replayed with ten actual selected translation units instrumented with ASan/UBSan;
C++ vptr and leak checks are disabled, dependencies remain uninstrumented. Seven
Painter C++ units inherit `-fno-rtti`; two core C++ units retain the default RTTI
flags, and one fixture is C. No whole-application no-RTTI claim is made.
Independent native fixture arithmetic uses shared RGB extrema and integer floor;
the predecessor packages prove this against literal pinned old C on defined
inputs. No old Edge/string application execution is included.

The expected-red executable retains the committed core without the new route and
must fail for the missing literal mapping, not merely exit nonzero. Four native
mutations test extra-slot admission, missing captured ROI, projection extent
policy and literal route dispatch. Original fixture-name/count mistakes and a
private runtime-library-path setup failure are preserved separately; they are
not claimed as product regressions. The corrected runtime explicitly uses the
same clean build libraries rather than relying on relocated `$ORIGIN` paths.

XCF/parameter-editor/portrait proof, indexed palette ownership and broader
Normalize support remain open. This is bounded progress on original 16.022,
not completion of that task or all Filter routes.
