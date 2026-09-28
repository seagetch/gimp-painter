# CloneLayer / FilterLayer の GIMP 3 vfunc 契約

旧版の `app/core/gimpclonelayer.cpp` と `app/core/gimpfilterlayer.cpp` にある
`NewGClass::bind` の実 binding 34 件を、旧版 `afa43fae` と現在の GIMP 3 の
C ヘッダーの slot 宣言で比較した。行ごとの署名・出典と担当タスクは
[`gimp3-layer-vfunc-review.tsv`](gimp3-layer-vfunc-review.tsv) に記録した。
`DONE` は**署名の照合完了**を意味し、vfunc の移植完了を意味しない。

| 判定 | 件数 | 主な変更 |
|---|---:|---|
| 同じ署名 | 18 | 個別の所有権・動作確認は担当する機能タスクで実施 |
| 署名変更 | 11 | `translate` の座標型、`resize` の fill type、`transform` の引数、メモリー見積り、opacity 戻り値、progress 開始の引数順 |
| slot 廃止 | 3 | 2 レイヤーの `is_editable`、FilterLayer の `project_region` |
| GLib 外部 ABI | 2 | `GObjectClass.constructed` は比較対象の GIMP ヘッダー外 |

## 投影への接続

旧 FilterLayer は `gimpfilterlayer.cpp:287` の `project_region` を
`GimpDrawableClass` に接続し、投影タイルの準備と runner 開始条件を処理していた。
現行 `app/core/gimpdrawable.h` にはこの slot がなく、
`get_source_node(GimpDrawable *) -> GeglNode *` がある。
既定実装 `app/core/gimpdrawable.c:1147` は drawable の `GeglBuffer` を
`gimp:buffer-source-validate` へ渡す。同ファイル `:1734` は source node と
drawable filter stack を接続し、`app/core/gimplayer.c:791` はその出力を
レイヤーの座標移動、合成 mode、mask に接続する。

移植では FilterLayer の runner が**確定した結果**を buffer/source node に供給し、
更新範囲を通知する。runner の起動や入力の確定判定は operator の評価から
行わない。未確定・旧世代の結果を表示しない条件は
`15.006/gegl-result-source` と実行制御タスクで扱う。旧 CloneLayer の
`gimpclonelayer.cpp:555` は source 投影から旧 tile へコピーしていたため、
参照 source の変更を現行 graph に伝える境界は `14.003/gegl-source-node` で扱う。
両者とも独自レイヤーの保存形式と再編集可能性を維持する。

## ABI と操作上の差分

| 差分 | 新しい契約 | 移植時の確認 |
|---|---|---|
| `GimpItemClass.translate` | `gint` 座標から `gdouble` 座標 | CloneLayer `14.006`、FilterLayer `15.005` |
| `GimpItemClass.resize` | `GimpFillType` を追加 | 同上。引数位置と塗りの意味を確認 |
| `GimpItemClass.transform` | recursion level 引数を削除 | 同上。旧引数を誤った位置へ渡さない |
| `GimpDrawableClass.estimate_memsize` | `const` を外し `GimpComponentType` を追加 | `14.003/gegl-source-node`、`15.006/gegl-result-source` |
| `GimpPickableInterface.get_opacity_at` | `gint` から `gdouble` へ変更 | 旧ゼロ透明の扱いを `14.003/clone-pickable-opacity`、`15.006/filter-pickable-opacity` で検証 |
| `GimpProgressInterface.start` | `cancellable` と `message` の順を変更 | `15.006/filter-progress-start` |
| `GimpItemClass.is_editable` | slot 廃止 | `14.006/editability-contract`、`15.006/editability-contract` で現行のロックと許可操作を検証 |

照合は C ヘッダーの静的な宣言比較である。GLib の外部定義、callback の実際の
呼出し時の所有権と生存期間、画素の結果、実行速度はここでは確認していない。
残る class callback 40 件と別の Binder 37 件は
`01.005/gimp3-vfunc-contract` の継続対象である。
