# Undo、canvas、JSON 資源の Binder slot

旧 Binder のうち、GIMP の現行 C ヘッダーで宣言を直接比較できる残りの 6 件を
[`gimp3-core-binder-review.tsv`](gimp3-core-binder-review.tsv) に記録した。
同じ署名 3 件、引数が変わった署名 3 件。`DONE` は宣言の照合を表し、
実装・実行試験の完了を表さない。

| 旧 binding | GIMP 3 の差分 | 担当タスク |
|---|---|---|
| CloneLayerUndo `pop` | `GimpUndoMode` と accumulator を含め、宣言は同じ | `08.004` |
| PerspectiveGuide canvas `draw` / `get_extents` | 2 callback とも `GimpDisplayShell *` 引数を削除。必要な shell は `gimp_canvas_item_get_shell(item)` で取得できる | `26.005` |
| JSON resource `save` | `GOutputStream *output` を追加。`app/core/gimpdata.c:639` の `gimp_data_save` が `g_file_replace` で開き、callback 後に close/unref するため、callback は引き渡された stream に書く | `28.002` |
| JSON resource `get_extension` / `duplicate` | 宣言は同じ。コピーの深さと所有権は別途検証 | `28.002` |

この監査のため、`tools/audit_gimp3_layer_vfuncs.py` の宣言解析を
`const gchar *` 戻り値にも対応させた。先に確認したレイヤー 34 件と
操作ツール 18 件の監査結果が変わらないことも再実行して確かめる。

残る Binder 13 件は、旧独自型の signal slot と GTK/GLib の callback を含む。
現行 GIMP の C ヘッダーとの署名比較だけでは完了にできないため、
`01.005/gimp3-vfunc-contract` の未完了対象として保持する。
