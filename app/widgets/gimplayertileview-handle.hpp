/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_LAYER_TILE_VIEW_HANDLE_HPP
#define GIMP_LAYER_TILE_VIEW_HANDLE_HPP
#include "gimplayertileview.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpLayerTileView> {
  static GType type () noexcept { return gimp_layer_tile_view_get_type (); }
};
/* Owns native widget storage. A destroyed widget remains terminal. */
class LayerTileViewRef {
public:
  static LayerTileViewRef create () {
    return LayerTileViewRef (ObjectRef<GimpLayerTileView>::sink (
      gimp_layer_tile_view_new ()));
  }
  static LayerTileViewRef retain (GimpLayerTileView *view) {
    return LayerTileViewRef (ObjectRef<GimpLayerTileView>::retain (view));
  }
  GimpLayerTileView *get () const noexcept { return owner_.get (); }
private:
  explicit LayerTileViewRef (ObjectRef<GimpLayerTileView> owner)
    : owner_ (std::move (owner)) {
    if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected layer tile view");
  }
  ObjectRef<GimpLayerTileView> owner_;
};
}
#endif
