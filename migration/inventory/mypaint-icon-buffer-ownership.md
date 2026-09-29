# 01.007/icon-buffer MyPaint アイコンの画素寿命

旧 `app/core/gimpmypaintbrush.cpp:756-772` の `set_icon_image` は既存の `icon_image` を destroy し、新しい `guchar* data` を `g_new` で確保して Cairo image surface を `cairo_image_surface_create_for_data` で作る。旧 destructor (`:471-475`) は surface を destroy するが、data の `g_free` は登録していない。同じ旧ツリーの `app/widgets/gimpcairo.c:76-80` はこの API で作った surface に `cairo_surface_set_user_data(..., g_free)` を付け、画素バッファーを surface の寿命に結び付けている。

旧 setter は確保量に `cairo_format_stride_for_width(format, width) * height` を使う一方、surface 作成には入力画像の `stride` を渡す。入力 stride が最小値より大きい場合、Cairo に渡した領域が `stride * height` より短くなる。また、`set_icon_image(icon_image)` は source を読む前に同じ surface を destroy する。移植先の `19.013/icon-buffer` では先に入力の参照または画素を確保し、stride に合う領域を確保し、data の解放を surface に結び付ける。異なる stride、同一 surface 再設定、複製、destructor を検証する。これは旧ソースの静的な契約追跡であり、移植実装ではない。
