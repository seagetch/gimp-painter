# Smudge native mask format boundary

Native hard-mask preparation returns `Y float`; the independent legacy Smudge
compositor consumes one byte per pixel. Normalize a non-byte final mask into a
Smudge-owned `Y u8` buffer after optional paper (which supports and preserves both
formats). Preserve native provider APIs and the existing byte-mask fastpath.

Prove the original misinterpretation fails the exact same independent old-source
center fixture, and that the bounded conversion restores expected bytes. Verify
odd/even masks, paper/no-paper, four formats, initial interior/edge/outside/negative
contacts and rates0/50/100, actual dab counts and full Undo/Redo. Re-run immutable
old48-scene and1024-kernel regressions normally and with scoped ordinary sanitizers.

This is a bounded original23.002 progress checkpoint, not closure of all52 duties,
23.003 blending,23.004 dynamic allocation, partial-alpha or full old-raster parity.
No old runtime or physical-device/UI execution is implied by source-only oracle.
