/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_NATIVE_POPOVER_HPP
#define GIMP_NATIVE_POPOVER_HPP
#include "popupper.h"
#include "painter/object-ref.hpp"
namespace GimpPainter GIMP_PAINTER_PRIVATE {
template<> struct TypeTraits<GimpPopover> {
  static GType type () noexcept { return GIMP_TYPE_POPOVER; }
};
class PopoverRef {
public:
  PopoverRef () noexcept = default;
  static PopoverRef create (GtkWidget *view) {
    GtkWidget *widget = gimp_popover_new (view);
    return retain (widget ? GIMP_POPOVER (widget) : nullptr);
  }
  static PopoverRef retain (GimpPopover *popover)
  { return PopoverRef (ObjectRef<GimpPopover>::retain (popover)); }
  GimpPopover *get () const noexcept { return owner_.get (); }
  explicit operator bool () const noexcept { return bool (owner_); }
  /* Releasing this extra reference does not close GTK's managed top-level. */
  void reset () noexcept { owner_.reset (); }
private:
  explicit PopoverRef (ObjectRef<GimpPopover> owner) noexcept : owner_ (std::move (owner)) {}
  ObjectRef<GimpPopover> owner_;
};
}
#endif
