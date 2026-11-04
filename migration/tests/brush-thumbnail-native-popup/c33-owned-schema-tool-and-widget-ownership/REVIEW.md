# C33 review candidate: owned schema compiler and bounded widget ownership repair

Base: published C32 9827beb9d354b7b809837eb2ec9008434cbe0664.
Run50 matched preparation exited127 at dependency-schemas; measurement skipped.
Only six artifact files were uploaded; private compiler stderr is not among them.
Exact hosted reason for exit127 is therefore not directly recovered.

Supported mechanism: the workflow runs pinned Debian13-slim, not the Ubuntu host
userspace. Its apt bootstrap installs libglib2.0-0t64 but does not install
libglib2.0-bin (confirmed in full job package log). Official Debian package lists
place the real compiler at usr/lib/x86_64-linux-gnu/glib-2.0/glib-compile-schemas
in0t64, while usr/bin/glib-compile-schemas is supplied by bin. C32 invoked the
host usr/bin frontend before sourcing the extracted dependency environment.
The immutable dependency lock includes both packages. This supports a missing
host frontend as the explanation, not an observed private stderr finding.

C33 selects only the regular executable at the already-extracted internal path
under the owned dependency root, and gives only that child the exact extracted
multiarch library directory. No system package install, path fallback, parent
environment mutation or schema/identity validation relaxation is introduced.
The cache remains a shared pre-build fixture, before all identities/measurements.
Native assertions/full107, timing inputs and all acceptance policies are untouched.

A new finite receipt records preflight/preparation stage, raw compiler exit,
compiler/cache hashes, and a bounded128KiB diagnostic hash/truncation flag plus
boolean loader-error observation. Raw query/path/environment/output is not exported.
Missing or unsafe compiler/root/output, launch error, timeout, compiler failure,
empty output and receipt-write failure all reject preparation. Private stderr
stays in the existing private log; no retry/fallback.90s compiler limit is inside
existing outer preparation deadline.

Proof: actual installed GLib2.84.4 compiler copied to the exact owned internal path,
with usr/bin frontend absent, generated a valid tiny schema cache read by actual
GSettings. This synthetic proof uses available system fallback libraries; it is
NOT proof of the actual locked extracted-library closure or hosted C33 success.
Unit tests cover injected loader rejection, missing/non-executable/unsafe inputs,
compiler failure/loader marker/truncation, negative signal status, timeout, missing
output, exact argv/child loader environment, shell failure propagation and receipt
write failure without raw exceptions.

Source references:
https://packages.debian.org/trixie/amd64/libglib2.0-0t64/filelist
https://sources.debian.org/src/glib2.0/2.84.4-3~deb13u3/debian/libglib2.0-bin.install/

No denied ptrace/babl execution or remote writes occurred. Native full107 proof
from C31/C32 is preserved; terminal run50 findings are recorded below.

## Narrow widget ownership repair

Two production changes free the temporary selected GtkTreePath list in
`gimp_container_tree_view_get_selected`, and the copied model string after
computing label length in `gimp_scale_combo_box_set_scale`. Returned item ordering,
borrowed viewables, model strings, width and lifetime contracts are unchanged.
The corresponding run49 stacks account for 172 bytes (72 direct + 60 indirect
selection paths, 40 label) in cases01/05/21 and fixture-no-popup. This is NOT a
repair claim for the roughly200KB aggregate leak report, nor popup-only attribution.

Run the reproducible normal ownership probe from repository root:

```
python3 -B migration/tests/brush-thumbnail-native-popup/c33-owned-schema-tool-and-widget-ownership/run-ownership-proof.py .
```

It extracts both complete current function bodies, compiles with strict warnings,
and uses real installed GTK3/GLib list-store/string/path APIs plus headless UI/GIMP
stubs and local ABI declarations (64-bit Linux; runtime shared libraries required).
Temporary outputs live outside source and are cleaned. Checks cover empty/multiple
selection, NULL items, filter/order/count, repeated and inserted scale, null label
fallback and widths. Current functions pass603/603 path and102/102 string frees;
each missing-free mutation fails the corresponding exact balance assertion.
This is bounded ownership evidence, NOT a full GTK widget/GIMP build, GUI, ASan or
LSan acceptance. Hosted full107 and strict sanitizer remain required.

Both changed production files are mandatory runtime-seal source entries, with
omission-negative tests. Specialized mask/frame provider source pins and historical
old-control fixtures are unchanged; their actual archives/loaded providers remain
dynamically hashed. All26 protected common files and historical receipts remain
unchanged. No sanitizer suppression, deadline extension or gate relaxation.

Ownership references:
https://docs.gtk.org/gtk3/method.TreeSelection.get_selected_rows.html
https://docs.gtk.org/gtk3/method.TreeModel.get.html

Task308 remains DOING / NOT ACCEPTED. The same-task successor retains sole accepted
parent e25a94e21efca6e6604ddac6f15570c049f421e7; this combined candidate is C33,
not a separately published C34.

## Run50 terminal receipt

Exact published C32 source:9827beb9d354b7b809837eb2ec9008434cbe0664.
See run50-terminal-observation-summary.json, SHA256 4ab08b27b37ed33404dee26df9538aaa4f1cfaae7981de157ecf80de495cd8d4.
Run50 completed all8 jobs:5success/3failure. Normal mask full107 aggregate proof
passes. Matched preparation stopped at dependency-schemas exit127 with zero
measurements. Sanitizer passed20 assertions but failed with199965–202605bytes of
leaks; case14 stable_count0 aborted at12.324995s (whole dispatch12.321169s).
Mask sanitizer focused leaf passed but process failed ASan/LSan; old/full unrun.
Eight bounded GDB samples and successful cleanup do not prove same-wait identity.
This immutable historical receipt is not a C33 execution result. All original
run50 artifacts/logs are retained in the separate recovery supplement.

Local combined source suite:1081 tests PASS;51 Python AST parses,9 shell syntax
checks and26 protected common-file hashes PASS. The source-extracted ownership
probe passes and both missing-free mutants fail. Full hosted revalidation pending.
