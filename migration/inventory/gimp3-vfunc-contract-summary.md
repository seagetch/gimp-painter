# 旧 vfunc と GIMP 3 の静的な契約照合

`01.005/gimp3-vfunc-contract` の対象である旧 Binder 71 件と直接 class
callback 40 件を、`afa43fae` の定義・登録箇所と現行 GIMP 3、固定版の
GTK/GLib 公開 C 宣言に対応付けた。各 TSV の `DONE` は**調査の完了**であり、
GIMP 3 への移植・描画動作・実行時の生存期間は別タスクで検証する。
`python3 -B tools/check_gimp3_vfunc_coverage.py` は元の 71+40 件と
七つの照合表の行を重複なく一対一に照合する。

| 対象 | 件数 | 行ごとの照合表 | 明らかになった移植境界 |
|---|---:|---|---|
| CloneLayer / FilterLayer Binder | 34 | `gimp3-layer-vfunc-review.tsv` | FilterLayer の `project_region` と両レイヤーの `is_editable` が廃止。GEGL source node、引数・戻り値変更を個別タスクへ |
| paint と操作 tool Binder | 18 | `gimp3-tool-vfunc-review.tsv` | 塗りつぶしブラシの `paint` が drawable 単体・座標から drawable 群・symmetry に変更 |
| Undo・canvas・JSON Binder | 6 | `gimp3-core-binder-review.tsv` | canvas の shell 引数 2 件を削除。JSON save に `GOutputStream*` を追加 |
| GTK/GLib と旧独自型 Binder | 13 | `gimp3-external-binder-review.tsv` | GTK cell renderer 3 件が変化。旧独自型の signal slot 3 件は型の作成待ち |
| MyPaint 資源の直接 callback | 15 | `gimp3-brush-callback-review.tsv` | preview と save の署名変更、独自型の slot 4 件を再作成 |
| MyPaint tool の直接 callback | 10 | `gimp3-tool-callback-review.tsv` | 署名は同じ。tool が保持する core と qdata の寿命を別途定義 |
| 共通ブリッジ・Undo・options・editor の直接 callback | 15 | `gimp3-remaining-callback-review.tsv` | 署名は同じ。旧 property 例外時の `exit(1)` と Undo pop 空処理は修正が必要 |

古い C++ class callback 40 件はすべて旧関数**定義**の署名が旧 slot と
一致することまで照合した。移植先の callback 定義はまだ作成していない。
GTK/GLib は監査表にリンクした GNOME 公開ヘッダーの固定版を参照し、
実機の導入版と同一であるとは仮定しない。

## 所有者と例外境界

共通 Binder の object / Impl / parent class の寿命は `01.005/bridge-core`、
間接 callback は `01.005/indirect-callback-owners` に記録した。
本照合で見つかった所有権上の相違は各結果表から
`08.008/tool-core-lifetime`、`19.013/preview-contract`、
`19.011/output-stream-save`、`09.011/mypaint-undo-stroke`、
`29.006/popover-class-signals` などの実装タスクへ割り当てた。
静的な署名一致だけでは C++ callback が C ABI に例外を漏らさないことを
保証できない。旧 `GClassWrapper` の property callback は catch 後に
`exit(1)` を実行するため、`05.013/legacy-exit-removal` で置換する。
全111件の移植先で失敗の戻し方と破棄経路を確かめる作業は
`05.013/all-vfunc-exception-containment` の完了条件に含める。

## 描画への影響

旧投影タイルを扱う `project_region` は現行 drawable の slot ではなく、
drawable source node、filter stack、レイヤー合成の GEGL 経路を使う。
`14.003/gegl-source-node` と `15.006/gegl-result-source` で結果を接続し、
FilterLayer の実行器を operator 評価内で起動しない。
塗りつぶしブラシは `27.009/stroke-lifecycle` と
`27.007/paintcore-gegl-compose` で有界探索、結果 mask、現行 paint core
の合成を維持する。これらの移植と旧版との画素比較は未完了。
