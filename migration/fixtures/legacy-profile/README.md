# Real legacy profile writer fixture

Captured with the recovered pinned old application's native GTK executable on
2026-10-02. The supplied `profile-seeds` are explicitly authored inputs; the
files under `profile/` are the **actual application writer output**, not hand
rewritten fixture output. `manifest.json` records the binary, seed, source-schema
and output hashes. All eight relevant old schema/serializer files match the
pinned source commit. Existing unrelated legacy build adaptations remain as
recorded in `migration/baseline/legacy/`.

The seed enables save-tool-options and save-device-status (the old default for
save-tool-options was false), restores the old shipped tool groups, and supplies
the shipped toolrc's missing registered ImageGenerator item. Without that item
the old application rejects its own shipped grouped toolrc and writes a flat
fallback. The placeholder remains hidden and is not claimed to be implemented
in the modern port.

The output includes 48 real per-tool option files, grouped toolrc, context,
devices and three custom accelerator bindings. MyPaint output retains .37
stroke opacity, non-incremental, use-gimp-texture and the deliberately missing
paper reference. The old writer did **not** emit the distinct Smudge seed values;
those are therefore covered by separate clearly synthetic tests, not presented
as genuine writer evidence. The legacy GTK/GValue warnings are retained in the
capture log. Successful fixture creation does not imply a clean legacy GUI or
physical-tablet test.
