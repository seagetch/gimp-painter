# GTK/GLib と独自型の Binder 境界

旧 `NewGClass` Binder の残り 13 件を、旧ソースの型宣言と GTK/GLib の公開
C ヘッダーで照合した。一件ずつの署名と固定版の行番号は
[`gimp3-external-binder-review.tsv`](gimp3-external-binder-review.tsv) に記録した。
これで Binder 71 件の行ごとの照合が揃った。表の `DONE` は静的な
境界の調査完了であり、GTK 3 の UI や独自型の実装完了を表さない。

外部 ABI の比較資料は GNOME のソース公開版：
[GTK 2.24.33 CellRenderer](https://raw.githubusercontent.com/GNOME/gtk/2.24.33/gtk/gtkcellrenderer.h)、
[GTK 3.24.43 CellRenderer](https://raw.githubusercontent.com/GNOME/gtk/3.24.43/gtk/gtkcellrenderer.h)、
[GTK 2.24.33 Widget](https://raw.githubusercontent.com/GNOME/gtk/2.24.33/gtk/gtkwidget.h)、
[GTK 3.24.43 Widget](https://raw.githubusercontent.com/GNOME/gtk/3.24.43/gtk/gtkwidget.h)、
[GLib 2.32.4](https://raw.githubusercontent.com/GNOME/glib/2.32.4/gobject/gobject.h) と
[GLib 2.80.0](https://raw.githubusercontent.com/GNOME/glib/2.80.0/gobject/gobject.h)。
これらは**比較するための固定版**であり、ビルド環境に入っている
GTK/GLib の版を断定するものではない。

| 判定 | 件数 | 判断と実装先 |
|---|---:|---|
| 同じ署名 | 7 | `GObjectClass.constructed` 4 件、`GtkWidgetClass` の map・button・key 3 件。`08.004` と 29 群で親処理・接続の寿命も確認 |
| 変更した署名 | 3 | `GtkCellRendererClass.activate` は矩形 2 引数を const 化、`get_size` は矩形を const 化、`render` は `GdkDrawable*` と expose 矩形を廃して `cairo_t*` を受ける。`29.006/cellrenderer-gtk3` で実装 |
| 移植先の独自型が未作成 | 3 | 旧 `GimpPerspectiveGuideClass.removed`、旧 `GimpPopoverClass.cancel` と `confirm` は GTK/GIMP 標準 slot ではない。`08.009/removed-signal` と `29.006/popover-class-signals` で型登録・発火条件を復元 |

旧 callback の宣言が同じでも、イベント伝播と描画順序、親 class への
チェーン、登録解除の動作一致は保証できない。GTK 3 で widget の動作と
旧 UI を比較し、C++ からの callback は例外を C ABI に通さない。
独自型の 3 件は**新しい GIMP 3 の署名がまだ存在しない**ことを
表の `TYPE_NOT_PORTED` で示す。削除済みの標準 slot とみなさない。
