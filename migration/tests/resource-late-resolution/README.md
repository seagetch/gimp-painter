# Stroke-purpose native resource reference lifecycle (19.008–19.010)

No production change. The original requirements are exact legacy resource-name
identification, selecting the referenced paper, and retaining missing references
for later reselection. Original prerequisites stay open; this proof does not
close their dependency chain or any held preview/native extended-brush work.

## Expected policy, audited before testing

At baseline afa43fae3e920210146abed514f136fd49f671b5,
`app/paint/gimpmypaintoptions.cpp:431–530` resolves exact names on enabled,
specified option updates, retaining context fallback when absent. The old
`app/paint/gimpmypaintcore.cpp:160–175` attaches context brush/paper at stroke
start. Current `GimpResources` Stroke construction resolves names for each new
owner. Committed20b PaintCore constructs those owners at begin and transient
hover (289/376). No subscription-driven rebinding of an existing owner is
promised or introduced. ResourceResolution names are construction-time reports,
not live rename notifications. Generic native PaintOptions context reselection
is tested; extended options UI/property synchronization is not rerun here.

## Matrix and observed results

Two native groups and the same two groups with eight selected translation units
instrumented by ASan/UBSan pass. The lifecycle group saves/reopens missing
Unicode brush/paper names, observes fallback, inserts named resources, and
proves only newly constructed owners resolve them. It explicitly reselects
context resources, renames original resources, observes missing-name fallback,
adds same-name replacements, removes both generations from factories, and
checks retained identities using brush-use counts and weak-finalization of
both brush and pattern objects. The requested Resource remains unchanged and
reopens with its original names. Factory counts and global brush/pattern
context identities are restored; the standard MyPaint factory count is unchanged.
The flags group covers disabled use, unspecified names, empty names and null
names without inventing missing-resource errors.

Two independently compiled lookup mutants force the successfully located brush
or paper pointer to null. Both fail the expected effective-resource assertions.
No preview constructor, held hook, surface drawing or GUI is invoked. This is
native Stroke-owner resolution, not a new full PaintCore stroke/pixel claim.
There is no automatic live-rebinding, deep-standard-resource equality, full
application shutdown, leak-check or whole-application sanitizer claim.

## Frozen inputs and execution boundary

The clean build was known20b plus the committed Options-mode overlay and an
explicit independently sealed Filter-rotate candidate. Selected native Resource,
GimpResources, Brush, BrushGenerated, Pattern, Data and Object providers match
20b exactly. All nine held candidate paths in that clean source also match20b;
they were never edited or used as candidates. Filter dependency seals are
retained explicitly; this is not a whole-HEAD claim.

Compilation and linking occurred in a private output directory under the clean
build lock, without clean-source/build changes. 1,322 linked input files
(including regular archives as blobs and thin archive members individually)
and127 loader-reported dynamic libraries were hashed before and after, with no
drift. Compiler dependency hashes are recorded. Only seven provider units plus
the new test are instrumented; remaining native objects/dependencies are not.
C++ vptr instrumentation and LeakSanitizer are disabled. Native tests use private
HOME/XDG/TMP directories and the existing isolated test-profile helper.

Replay requires a reviewed clean source/build and its environment: hold its
`build.lock`, then invoke `build.py --build <clean-build> --output <private-dir>`.
Do not rebuild the clean tree or substitute dirty providers. `validate.py
--check-source` validates immutable receipts and selected current sources without
running native tests. No task ledger or production source is changed here.
