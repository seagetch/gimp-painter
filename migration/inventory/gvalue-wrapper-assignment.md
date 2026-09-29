# 01.007/value-wrapper GValue の再代入

旧 `app/base/glib-cxx-utils.hpp:651-671` で `HoldValue` は確保済み `GValue*` の中身を `g_value_unset` し、領域を `g_free` する。`CopyValue` の各構築子 (`:699-779`) は `g_new0` した一時値を初期化し、参照を持つ型も `g_value_copy` または setter で複製する。

旧 `CopyValue::operator=(const CopyValue&)` (`:781-786`) は保持中の `GValue` を `g_value_unset` せず `G_VALUE_INIT` で上書きする。文字列や GObject の旧値の保持が失われる。自己代入では右辺も同じ領域なので、型情報を消した後に `G_VALUE_TYPE(src.obj)` を使う。旧 `CopyValue::operator=(CopyValue&&)` (`:788-791`) は代入先の旧値を解放せず、中身を浅くコピーして `src.obj = NULL` とする。移動元の `g_new0` 領域は解放されず、移動先の以前の参照も失われる。

`FundamentalTraits::cast` (`:628-631`) は変換先の局所 `GValue` を unset せずに返る。変換先が所有型の場合に一時参照が残る。これらは移植先では `06.020/value-assignment` で borrow / owned / moved の状態を分け、旧値の解放と自己代入を含むテストを行う。旧コードの静的な経路追跡であり、GIMP 3 側の修正・ランタイム検証はまだ行っていない。
