# Original Perspective stroke acceptance checkpoint

26.007 描画方向の決定を移植する — 旧判定距離と優先方向が一致 (dependency26.006).
26.008 筆跡拘束を移植する — 確定方向へ入力座標を射影 (dependency26.007).
26.009 stroke 終了時の拘束解除を実装する — 次の stroke に状態が漏れない (dependency26.008).

The native entry checkpoint in `migration/tests/perspective-stroke-entry/`
compares unchanged pinned old arithmetic on explicitly shared current input
coordinates. Original87 whole-hunk duties, criteria and dependency order remain
open; no broader smoothing/symmetry/physical-device/runtime assertion is implied.
The separate target preserves the prior editor13 source and registrations.
