/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_JSON_RESOURCE_HPP
#define GIMP_JSON_RESOURCE_HPP
extern "C" {
#include "gimpjsonresource.h"
}
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpJsonResource>
{ static GType type () noexcept { return gimp_json_resource_get_type (); } };
class JsonResourceRef {
public:
  JsonResourceRef () noexcept = default;
  static JsonResourceRef adopt (GimpJsonResource *p)
  { return JsonResourceRef (ObjectRef<GimpJsonResource>::adopt (p)); }
  static JsonResourceRef retain (GimpJsonResource *p)
  { return JsonResourceRef (ObjectRef<GimpJsonResource>::retain (p)); }
  static JsonResourceRef create (GimpContext *context, const char *name)
  { return adopt (GIMP_JSON_RESOURCE (gimp_json_resource_new (context, name))); }
  GimpJsonResource *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  void reset () noexcept { owner_.reset (); }
  gboolean set_json (JsonNode *node) const { return gimp_json_resource_set_json (get (), node); }
  /* Transfer-full node, matching the C provider; caller releases with json_node_unref. */
  JsonNode *ref_json () const { return gimp_json_resource_ref_json (get ()); }
private:
  explicit JsonResourceRef (ObjectRef<GimpJsonResource> value) noexcept : owner_ (std::move (value)) {}
  ObjectRef<GimpJsonResource> owner_;
};
}
#endif
