# Original switch declared-default parity (24.003 partial)

Only the native Options class-init boolean GParamSpec default changes from a
literal FALSE to the already pinned original `s.default_value`: use_gimp_brushmark
TRUE, the other four FALSE. Resource missing/constructor switches remain FALSE;
no CONSTRUCT flag, property range, ID, name, serialization implementation,
selection/readiness/acquisition path, renderer, or owner policy changes.

## Actual behavior and compatibility
- Causal original provider red: declared/reset default was FALSE and metadata
  TRUE was unnecessarily serialized. Exact old source/metadata is archived.
- Normal 4 and three-production-unit ASan/UBSan 4 pass (Options, Resource,
  BindingStore, plus fixture). Other dependencies uninstrumented; leaks disabled.
- Full current Options suite 28 passes: original 24 plus four new cases, no skips.
  Existing scalar reset expectation now uses original metadata, not blanket FALSE.
  No paused ref_for_context acquisition-reentry test is present or invoked.
- Actual prechange native TRUE/FALSE, full/changed serializer files are preserved.
  All four reopen correctly after new reset: `painter-settings` JSON preserves
  FALSE even when the scalar is omitted. Removing that real JSON field causally
  changes omitted FALSE to reset TRUE. Scalar-only omitted config leaves receiver
  unchanged: fresh Resource FALSE, reset receiver TRUE. Do not claim exact old
  native scalar-only omission-after-reset parity or old CONSTRUCT semantics.
- Unchanged numeric fixture replay: baseline exact55bf 3 and candidate 3 pass,
  including both directions of all five switches and loaded6→5.9 recovery.
- Reset GUI 1: API reset asserts all five model values and two views, visible
  checkbox matches TRUE. `ui/reset-portrait.png` is actual Xvfb1080×1920 with
  explicit oncanvas set/get assertion, not pointer-driven interaction. Numeric
  portraits likewise explicitly enable oncanvas. Missing icon warnings retained.

## Scope and provenance
Base `55bfa5f1f72f33fb5a2f10b1f7a319bd773b0e94`, privately compiled/linked with
only the owned Options expression override. Linked inputs before/after identical;
full base source manifest included. This is not a new whole-HEAD aggregate.
Prechange serializer uses unchanged base Options. Old runtime proofs remain
immutable; a separate explicit persistence semantic successor is required before
current source gate admission (exactly one default field, no broad reseal).

Original 24.002/24.003 remain DOING with dependencies unchanged; old broader
editor parity, renderer and acquisition holds remain open. Initial fixture compile
mixed gboolean list, incorrect JSON-name expectation, whole-archive sanitizer
link failure and nested subgroup-after-application-close failure are preserved,
not counted as production regressions. Final exact fixtures/results are separate.

Run `PYTHONDONTWRITEBYTECODE=1 python3 migration/tests/options-switch-default/validate.py
--negative-controls` (optionally `--commit SHA`). Four evidence tamper controls
supplement—not replace—the causal native old-provider red and JSON-removal test.
