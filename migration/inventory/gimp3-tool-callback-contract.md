# 拡張 MyPaint tool の直接 class callback

旧 `app/tools/gimpmypainttool.cpp` の直接 class callback 10 件を、
旧 C++ の関数定義、旧 GIMP / GLib slot、現行 slot と照合した。
全10件の C ABI 上の引数と戻り値は同じ。
対応表は [`gimp3-tool-callback-review.tsv`](gimp3-tool-callback-review.tsv)。
`DONE` は署名の照合完了を表し、ツール動作の移植完了を表さない。

| 分類 | 対象 | 機能実装での確認 |
|---|---|---|
| ライフサイクル 2 件 | `GObjectClass.constructed` / `finalize` | 旧 constructed（`gimpmypainttool.cpp:169`）は `new GimpMypaintCore` を tool と qdata に保持する。旧 finalize（`:198`）では明示 delete がコメント化されている。現行で qdata と tool が同じ core を保持するときの所有者・破棄・停止処理を `08.008/tool-core-lifetime` で決める |
| 入力 callback 7 件 | `control`、`button_press`、`button_release`、`motion`、`modifier_key`、`cursor_update`、`oper_update` | 引数の宣言は同じ。GIMP 3 の tool manager と paint core、入力イベントから一筆への伝播を `08.008` と描画タスクで確認 |
| 描画 callback 1 件 | `GimpDrawToolClass.draw` | 宣言は同じ。現行 overlay の座標・canvas 更新へ接続し実画面で確認 |

GObject slot は GNOME 公開ソースの
[GLib 2.32.4](https://raw.githubusercontent.com/GNOME/glib/2.32.4/gobject/gobject.h) と
[GLib 2.80.0](https://raw.githubusercontent.com/GNOME/glib/2.80.0/gobject/gobject.h)
を固定して対照した。実装では C++ 例外を callback 境界で止め、親 class
の処理と終了時の参照解除を確認する。
