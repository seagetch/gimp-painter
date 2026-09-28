# 操作ツールと塗りつぶしブラシの vfunc 契約

旧 Binder 18 件（塗りつぶしブラシ 1、ImageGenerator placeholder 8、
パース定規 tool 9）と現行 GIMP 3 の C slot 宣言を照合した。
[`gimp3-tool-vfunc-review.tsv`](gimp3-tool-vfunc-review.tsv) に旧・現行の
ヘッダー行と署名を一件ずつ記録した。署名一致 17 件、変更 1 件。
`DONE` は宣言の照合完了であり、ツールの移植完了ではない。

変更されたのは `GimpPaintCoreClass.paint`。旧
`app/paint/gimppaintcore.h:86` は `GimpDrawable *drawable` と
`const GimpCoords *coords` を渡す。現行 `app/paint/gimppaintcore.h:97` は
同じ位置に `GList *drawables` と `GimpSymmetry *sym` を渡す。
旧 `app/tools/gimpbucketfillbrushtool.cpp:229` の `Brush::paint` は MOTION
で初回 snapshot を確保して motion を実行し、FINISH で旧 tile と状態を解放する。
現行の paint callback では描画対象群と対称座標を正しく扱い、
snapshot・範囲探索・paint buffer・一筆の Undo の寿命を
`27.009/stroke-lifecycle` と `27.007/paintcore-gegl-compose` で検証する。

パース定規の `GimpToolClass` 8 slot と `GimpDrawToolClass.draw` 1 slot は
C 宣言上の引数・戻り値が同じ。新しい入力や描画ライフサイクルでも同じ動作に
なるかは `08.010` と 26 群の機能・比較タスクで確認する。
ImageGenerator の 8 slot も宣言は一致するが、placeholder を移植する
要否は `31.011` の保存・依存調査で判定する。

ここでは C slot の宣言のみを確認した。各 C++ 実装の例外境界、
呼出し時の所有権、旧筆跡との画素一致は未確認。
