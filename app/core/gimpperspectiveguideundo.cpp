/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
extern "C" {
#include "core-types.h"
#include "gimpimage.h"
#include "gimpimage-undo.h"
#include "gimpimage-perspective-guide.h"
#include "gimpundo.h"
}
#include "gimpperspectiveguide.hpp"
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
using namespace GimpPainter;
typedef struct { GimpUndo parent; gboolean binding_failed; } GimpPerspectiveGuideUndo;
typedef struct { GimpUndoClass parent; } GimpPerspectiveGuideUndoClass;
GType gimp_perspective_guide_undo_get_type (void);
G_DEFINE_TYPE (GimpPerspectiveGuideUndo, gimp_perspective_guide_undo, GIMP_TYPE_UNDO)
namespace GimpPainter {
template<> struct TypeTraits<GimpPerspectiveGuideUndo>
{ static GType type () noexcept { return gimp_perspective_guide_undo_get_type (); } };
}
namespace {
struct UndoImpl { ObjectRef<GimpPerspectiveGuide> snapshot; void close () noexcept { snapshot.reset (); } };
struct UndoSlot : SlotSpec<GimpPerspectiveGuideUndo, UndoImpl> {};
void set_property (GObject *object, guint prop, const GValue *value, GParamSpec *pspec)
{
  if (prop != 1) { G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop, pspec); return; }
  boundary_void (nullptr, [&] { BindingStore::require (object).initialize<UndoSlot> ([&] (UndoImpl& impl) {
    impl.snapshot = ObjectRef<GimpPerspectiveGuide>::adopt (gimp_perspective_guide_duplicate (
      static_cast<GimpPerspectiveGuide *> (g_value_get_object (value))));
  }); });
}
void constructed (GObject *object)
{
  G_OBJECT_CLASS (gimp_perspective_guide_undo_parent_class)->constructed (object);
  boundary_void (nullptr, [&] { BindingStore::require (object).activate (); });
}
void dispose (GObject *object)
{ gimp_painter_binding_close (object, nullptr); G_OBJECT_CLASS (gimp_perspective_guide_undo_parent_class)->dispose (object); }
void pop (GimpUndo *undo, GimpUndoMode mode, GimpUndoAccumulator *accum)
{
  GIMP_UNDO_CLASS (gimp_perspective_guide_undo_parent_class)->pop (undo, mode, accum);
  boundary_void (nullptr, [&] { BindingStore::require (G_OBJECT (undo)).with<UndoSlot> ([&] (UndoImpl& impl) {
    auto current = ObjectRef<GimpPerspectiveGuide>::adopt (gimp_perspective_guide_duplicate (gimp_image_get_perspective_guide (undo->image)));
    auto restored = ObjectRef<GimpPerspectiveGuide>::adopt (gimp_perspective_guide_duplicate (impl.snapshot.get ()));
    impl.snapshot = std::move (current);
    gimp_image_set_perspective_guide (undo->image, restored.get ());
  }); });
}
}
static void gimp_perspective_guide_undo_class_init (GimpPerspectiveGuideUndoClass *klass)
{
  auto *object = G_OBJECT_CLASS (klass);
  object->constructed = constructed; object->dispose = dispose; object->set_property = set_property;
  GIMP_UNDO_CLASS (klass)->pop = pop;
  g_object_class_install_property (object, 1, g_param_spec_object ("guide", nullptr, nullptr, GIMP_TYPE_PERSPECTIVE_GUIDE,
    GParamFlags (G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)));
}
static void gimp_perspective_guide_undo_init (GimpPerspectiveGuideUndo *undo)
{
  undo->binding_failed = !boundary<bool> (nullptr, false, [&] {
    BindingStore::ensure (G_OBJECT (undo)).emplace<UndoSlot> (); return true;
  });
}
void gimp_image_perspective_guide_push_undo (GimpImage *image, GimpPerspectiveGuide *before, const gchar *name)
{
  g_return_if_fail (GIMP_IS_IMAGE (image));
  /* Rulers remain session-only as in the legacy application. Do not dirty XCF. */
  gimp_image_undo_push (image, gimp_perspective_guide_undo_get_type (), GIMP_UNDO_GUIDE,
                        name, GIMP_DIRTY_NONE, "guide", before, nullptr);
}
