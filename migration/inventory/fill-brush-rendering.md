# 塗りつぶしブラシの旧描画境界と GIMP 3 の接続候補

対象: `origin/gimp-2-8` の `app/tools/gimpbucketfillbrushtool.cpp` と
`app/core/gimpimage-contiguous-region.c`、移植先の `app/paint/gimppaintcore.h`、
`app/paint/gimppaintcore.c`、`app/core/gimppickable-contiguous-region.h`、
`app/core/gimppickable-contiguous-region.cc`、`app/core/gimpdrawable-bucket-fill.c`、
`app/tools/gimpbucketfilltool.c`。ここでは静的な呼出し境界を記録する。
旧版での実測と GIMP 3 での移植試験は未実施。

| 旧経路 | 確認した動作・入口 | GIMP 3 の境界と残作業 |
|---|---|---|
| GType と paint vfunc | `OptionsClass` と `BrushClass` が `GimpPaintOptions` と `GimpBrushCore` を継承。`paint` から初期化・押印・終了を呼ぶ | `GimpPaintCoreClass.paint` は `GList *drawables`、`GimpSymmetry *sym` を受ける。型登録、複数 drawable と symmetry の扱い、終了時の寿命を個別に決める |
| 投影の固定 | `start()` は projection を flush し、旧 `TileManager` を複製して開始色を保持。後続の dab はこの元画像を読む | `GeglBuffer` で開始時の**独立したスナップショット**を確保する。`gimp_pickable_contiguous_region_by_seed()` は内部で現在の pickable を flush・取得するので直接置換できない |
| dab の形 | 変形後ブラシの外接矩形と `gimp_brush_core_get_brush_mask()` の coverage を使う | 現行 `GimpBrushCore` の変形済み mask を取得して実画像・drawable 座標へ変換し、探索用の領域と coverage に使う。左右端・負の offset・rotation を試験する |
| 有界探索 | `gimp_image_contiguous_region_by_seed_full()` に矩形、参照画素、開始色、ブラシ mask を渡す。連続領域ヘルパーは当該矩形内で走査する | 現行 `gimp_pickable_contiguous_region_by_seed()` は pickable・seed・比較条件のみを受け、全面 extent に探索する。スナップショット・矩形・mask・固定開始色を渡せる GEGL buffer ベースの有界探索入口を追加する。共有する画素比較式は旧結果との一致を確認する |
| 選択境界 | 旧 `gimp_image_contiguous_region_by_seed_ext()` は選択 channel を source mask として `full()` に渡す | 現行標準 bucket fill は領域探索後に選択範囲と交差する。選択外での迂回を許さない旧入口を別途用意し、標準 bucket fill 側も旧差分を棚卸しする |
| 合成・Undo | 前景色または消去時の背景色で paint area を作り、探索 mask を `gimp_paint_core_paste()` に渡す。`GIMP_PAINT_CONSTANT` と Undo は stroke 単位 | `GimpPaintCore` の `GeglBuffer` paint buffer、canvas buffer と `gimp_paint_core_paste()` の現行署名に接続する。mask、成長 1 px、opacity、消去 mode、Undo、dirty 領域と projection を個別に比較する |
| ツールと UI | 独自 tool/options を登録し、rate と eraser-mode を設定できる | paint core と tool/options の GType と登録を移植する。旧 `rate` / dynamic rate は計算するが同じ `motion()` から使用が見えないため、効果を断定せず旧版を測定する。旧探索閾値 30 はこのツールの UI 設定ではない |

標準の `GimpBucketFillTool` はプレビューに `gegl:buffer-source` と
`gegl:translate` のグラフ、および一時 `GimpDrawableFilter` を利用する。
これはブラシの有界領域探索を代替する証拠にはならない。旧版は
`gimp_image_contiguous_region_by_seed_full()` に固定閾値 30、
`GIMP_SELECT_CRITERION_COMPOSITE` と開始色を渡す。閾値、alpha、
アンチエイリアス、ブラシ coverage の算出順を実際の旧結果と比較する。

判断: 探索は stroke/dab 側の有界な buffer 処理とし、その結果 mask を
現行 paint core の合成に渡す。各 dab の領域探索を投影グラフの
`GeglOperation.process` から開始しない。新規 GEGL Operator が必要かどうかは
グラフ経由で遅延評価する他機能と混同せず、合成と投影接続の調査で決める。

比較試験では少なくとも、ブラシ矩形の外側を通る U 字状の同色経路、
選択範囲の外側で接続する二領域、負の drawable offset、回転した柔らかい
ブラシ mask、半透明の開始画素、dab を跨ぐ開始画像の固定、キャンセルと
Undo / Redo を別々に記録する。これらの旧版 fixture はまだ採取していない。
