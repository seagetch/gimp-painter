# Original Perspective editor acceptance checkpoint

Original26.002: 消失点の追加を実装する — 上限と初期位置が旧仕様どおり (dependency26.001).
Original26.003: 消失点の移動を実装する — 画像座標へ変換して保持 (dependency26.002).
Original26.004: 消失点の削除を実装する — 残りの参照と選択が整合 (dependency26.003).
Original26.006: 消失点 hit test を移植する — 画面縮尺を考慮して選択 (dependency26.005).

`migration/tests/perspective-editor-entry/` supplies actual canvas-entry evidence,
not merely tool-vfunc tests. The62 original work rows and shared whole-hunk
owners remain unmodified/open; prerequisites are not silently closed. The exact
legacy13px circle policy is source-only and bounded to CIRCLE/CENTER with one
point; nearest-selection assertions are separately source-audited. No physical
pen or old runtime requirement is invented or asserted satisfied. Existing
26.005 display proof remains independently sealed and unchanged.
