# 01.007/surface-regions

旧 `app/paint/gimpmypaintcore-surface.cpp` の raw 候補14件は、`PixelRegion` の `g_free` が12件、Surface factory の `new` が2件。`configure_pixel_regions` (`:209-258`) は `GeneralDrawableFeature::get_tiles_region` / `get_temp_buf_region` (`gimpmypaintcore-drawablefeature.hpp:5-25`) または `g_new(PixelRegion, 1)` で一時領域を返す。region 構造体のメモリーは呼出側が所有し、背後の TileManager / TempBuf は feature または資源が所有する。

`draw_dab_impl` は `src1PR`、`destPR`、`brushPR`、`maskPR`、`texturePR` を NULL 初期化し、非累積描画時に先の三つを解放して別 region に交換 (`:327-335`) する。最後に五つを解放 (`:345-349`) する。`get_color_impl` は read 用の四つを取得し、採色後に解放 (`:400-403`) する。`prepare_brush` / `adjust_boundary` による早期 return は取得より前である。描画中の処理で例外や早期 return を追加すると解放を失うため、移植先では scope owner を使う。

`GimpMypaintSurface_new` (`:614`) と `GimpMypaintSurface_TempBuf_new` (`:618`) は新しい C++ Surface を返す。前者は `GimpMypaintCore::surface` が通常終了・drawable 交換で削除し、後者は旧ブラシ preview の局所 `surface` が描画後に削除 (`app/core/gimpmypaintbrush.cpp:627,734`) する。GIMP 3 の `09.003/region-lifetime` では TileManager / PixelRegion そのものを引き継がず、GeglBuffer で source・作業領域・mask・texture・Undo snapshot の所有者と破棄順を定義する。静的な旧経路の記録であり、GEGL 実装と動作検証は未着手。
