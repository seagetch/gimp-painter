# ブリッジ・Undo・options・editor の直接 callback

旧 `vfunc-assignment-review.tsv` の残り 15 件を旧 C++ 定義と旧 slot、現行
GIMP 3 / GLib の C slot に照合した。全15件で引数・戻り値の署名は同じ。
一件ごとの行と実装タスクは
[`gimp3-remaining-callback-review.tsv`](gimp3-remaining-callback-review.tsv) に記録。
これで直接 class callback 40 件すべての署名照合が揃った。
表の `DONE` は**調査完了**を表し、callback の安全な移植は別の作業となる。

| 対象 | 件数 | ABI 一致後に必要な確認 |
|---|---:|---|
| 共通 `GClassWrapper` | 3 | `set_property` と `get_property` の旧 catch-all はログ後 `exit(1)`（`app/base/glib-cxx-impl.hpp:185,216`）。`05.013/legacy-exit-removal` で property の失敗を C 境界内に収める。`instance_finalize` は C++ destructor を呼んで親 finalize に接続（同 `:413`）。例外・再入・破棄順は `05.011` と 05 群で定義 |
| MyPaint core Undo | 5 | `pop`（`app/paint/gimpmypaintcoreundo.cpp:137`）は親処理後、stroke があっても空の分岐に入るだけ。`free`（`:152`）は生の Stroke pointer を delete する。`09.011/mypaint-undo-stroke` で実画素の Undo/Redo と pointer 所有を両立 |
| MyPaint options | 4 | dispose / finalize / property callback は署名一致。旧 finalize は brush を unref。新しい options の寿命と property ID を `08.007` で確認 |
| MyPaint brush editor | 3 | constructed / `GimpDataEditorClass.set_data` / `GimpDockedInterface.set_context` は署名一致。旧 editor は context の取得・options の型判定が不整合なため `24.001/editor-context-contract` で取得方法と解放を確認 |

GLib の外部 `GObjectClass` slot は GNOME 公開ヘッダーの
[2.32.4](https://raw.githubusercontent.com/GNOME/glib/2.32.4/gobject/gobject.h) と
[2.80.0](https://raw.githubusercontent.com/GNOME/glib/2.80.0/gobject/gobject.h)
の固定版で比較した。実行時に導入する GLib の版を断定しない。
子タスク群で署名を確認しても、C ABI に C++ 例外が伝播しないこと、
callback の親呼出しと owner の寿命は実装・試験で確かめる必要がある。
