# Original Perspective lifecycle acceptance checkpoint

26.010 定規編集 Undo を接続する — 追加・移動・削除を復元 (dependency26.009).
26.011 定規の保存対象を照合して接続する — 旧保存・非保存の境界を変更せず対応 (dependency26.010).
26.012 画像切替と終了を接続する — 別画像の点や signal を保持しない (dependency26.011).

The native checkpoint under `migration/tests/perspective-lifecycle-entry/`
exercises actual canvas entry, native Undo/Redo, XCF stream Save/Open, exact
controlled writer-byte non-persistence, duplicate, active image switch and close.
Original45 source duties and dependencies stay open; no broader closure or old
runtime inference. The current session-only Undo addition uses GIMP_DIRTY_NONE.
The pinned old complete XCF tree/duplicate absence is explicitly source evidence.
