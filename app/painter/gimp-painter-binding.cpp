/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "gimp-painter-binding.h"
#include "binding-store.hpp"

gboolean
gimp_painter_binding_close (GObject *owner, GError **error)
{
  return GimpPainter::boundary<gboolean> (error, FALSE, [owner] {
    if (!G_IS_OBJECT (owner))
      throw GimpPainter::Error (GIMP_PAINTER_ERROR_WRONG_TYPE, "Close requires a GObject owner");
    if (auto *store = GimpPainter::BindingStore::find (owner)) store->close ();
    return TRUE;
  });
}
