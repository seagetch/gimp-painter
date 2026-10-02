/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_CLONE_LAYER_HANDLE_HPP
#define GIMP_CLONE_LAYER_HANDLE_HPP
extern "C" {
#include "gimpclonelayer.h"
}
#include "gimp-painter-type-traits.hpp"
#include "painter/resources.hpp"

namespace GimpPainter {
/* A named, owning public handle. No Impl pointer or generic interface cast. */
class CloneLayerRef
{
public:
  static CloneLayerRef retain (GimpCloneLayer *layer)
  { return CloneLayerRef (ObjectRef<GimpCloneLayer>::retain (layer)); }
  static CloneLayerRef adopt (GimpCloneLayer *layer)
  { return CloneLayerRef (ObjectRef<GimpCloneLayer>::adopt (layer)); }
  static CloneLayerRef sink (GimpCloneLayer *layer)
  { return CloneLayerRef (ObjectRef<GimpCloneLayer>::sink (layer)); }
  static CloneLayerRef create (GimpImage *image, GimpLayer *source, gint width, gint height,
                              const char *name, double opacity, GimpLayerMode mode)
  {
    auto *layer = gimp_clone_layer_new (image, source, width, height, name, opacity, mode);
    if (!layer) throw Error (GIMP_PAINTER_ERROR_INVALID_STATE, "CloneLayer construction failed");
    /* GimpItem factories return a floating reference. Own it before a later
     * image insertion can sink it on behalf of the container. */
    return sink (GIMP_CLONE_LAYER (layer));
  }
  GimpCloneLayer *get () const noexcept { return owner_.get (); }
  void set_source (GimpLayer *source) const
  {
    GError *error = nullptr;
    if (!gimp_clone_layer_set_source_full (get (), source, &error)) fail (error);
  }
  void set_source_name (const char *name) const
  {
    GError *error = nullptr;
    if (!gimp_clone_layer_set_source_name_full (get (), name, &error)) fail (error);
  }
  ObjectRef<GimpLayer> source () const
  { return ObjectRef<GimpLayer>::retain (gimp_clone_layer_get_source (get ())); }
  String source_name () const { return String (gimp_clone_layer_dup_source_name (get ())); }
private:
  explicit CloneLayerRef (ObjectRef<GimpCloneLayer> owner) : owner_ (std::move (owner))
  {
    if (!owner_) throw Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Expected CloneLayer");
  }
  [[noreturn]] static void fail (GError *error)
  {
    Error failure (error ? static_cast<GimpPainterError> (error->code) : GIMP_PAINTER_ERROR_INVALID_STATE,
                   error ? error->message : "CloneLayer operation failed");
    g_clear_error (&error);
    throw failure;
  }
  ObjectRef<GimpCloneLayer> owner_;
};
}
#endif
