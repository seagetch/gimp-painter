/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_FILTER_LAYER_HANDLE_HPP
#define GIMP_FILTER_LAYER_HANDLE_HPP
extern "C" {
#include "gimpfilterlayer.h"
}
#include "gimp-painter-type-traits.hpp"
#include "painter/resources.hpp"
namespace GimpPainter {
class FilterLayerRef
{
public:
  static FilterLayerRef retain (GimpFilterLayer *layer)
  { return FilterLayerRef (ObjectRef<GimpFilterLayer>::retain (layer)); }
  static FilterLayerRef adopt (GimpFilterLayer *layer)
  { return FilterLayerRef (ObjectRef<GimpFilterLayer>::adopt (layer)); }
  static FilterLayerRef sink (GimpFilterLayer *layer)
  { return FilterLayerRef (ObjectRef<GimpFilterLayer>::sink (layer)); }
  static FilterLayerRef create (GimpImage *image, int width, int height, const char *name,
                               double opacity, GimpLayerMode mode)
  {
    auto *layer = gimp_filter_layer_new (image, width, height, name, opacity, mode);
    if (!layer) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "FilterLayer construction failed");
    return adopt (GIMP_FILTER_LAYER (layer));
  }
  GimpFilterLayer *get () const noexcept { return owner_.get (); }
  void set_definition (const char *procedure, GBytes *raw, const GimpValueArray *args) const
  {
    GError *error = nullptr;
    if (!gimp_filter_layer_set_definition (get (), procedure, raw, args, &error))
      {
        Error failure (error ? static_cast<GimpPainterError> (error->code) : GIMP_PAINTER_ERROR_INVALID_STATE,
                       error ? error->message : "FilterLayer definition failed");
        g_clear_error (&error);
        throw failure;
      }
  }
  String procedure () const { return String (gimp_filter_layer_dup_procedure (get ())); }
  GimpFilterLayerState state () const noexcept { return gimp_filter_layer_get_state (get ()); }
private:
  explicit FilterLayerRef (ObjectRef<GimpFilterLayer> owner) : owner_ (std::move (owner))
  { if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected FilterLayer"); }
  ObjectRef<GimpFilterLayer> owner_;
};
} // namespace GimpPainter
#endif
