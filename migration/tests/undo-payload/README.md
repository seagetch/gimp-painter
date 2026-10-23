# 09.017 actual native Undo payload

The new32-cell native test inspects the actual GimpDrawableUndo buffer, not a mock
or merely Undo stack depth. U16,U32,half,double ×RGB/Gray ×alpha/no-alpha ×normal/
floating strokes start from independently encoded native patterns with low-bit
information. Three sparse logical segments hit image edges and tile crossings.
The saved Undo payload must contain original native bytes before Undo; after the
swap it must contain the painted bytes; Redo restores the inverse state. Each
case repeats three exact Undo/Redo cycles, checking whole-image pixels, format,
extent and native lifecycle. All38 registered aggregate TAP cases pass, including
prior Surface cases, BufferRead8 and actual C TempBuf6.

The new group passes selected11-unit ASan/UBSan/float-cast-overflow. Dependencies
are not instrumented; native vptr and LSan are off. Two actual GimpPaintCore mutants
fail: storing the Undo snapshot in RGB float instead of native format, and copying
the painted target instead of the original snapshot. Both are restored and the
exact32-cell native group passes again. No production code changes are required.

All36 assigned original source duties retain exact identities and unchanged full
source-specific action/acceptance, with explicit reverse-linked future owners.
The original471/67-line MyPaintCore and164/50-line MyPaintCoreUndo definitions are
not declared implemented by generic native pixel history. Engine/stroke/settings/
random-state replay and native hook integration remain09.011/mypaint-undo-stroke;
scalar/iterator/consumer and representation replacement stays31.017 and existing
owners. No original inventory row is closed. Native model tests under portrait
Xvfb are not screenshot/UI evidence.

The primary build intentionally retained the last committed Engine checkpoint
while another worker develops exact engine-state restoration separately. Archive
and report bind46 exact source files, including that Engine and evaluator headers.
--check-source detects current changes; --check-committed-engine-checkpoint binds
only the three explicitly listed Engine blobs to their immutable committed SHA
and compares all other current sources. It does not bless the new Engine. Final
current-Engine aggregate and original engine/stroke proof remain required and
must not be replaced by these old receipts. No paused popup/preview/canvas source
was rebuilt by this bounded model test.
