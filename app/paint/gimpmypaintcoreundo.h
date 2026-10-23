/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef __GIMP_MYPAINT_CORE_UNDO_H__
#define __GIMP_MYPAINT_CORE_UNDO_H__
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "core/core-types.h"
G_BEGIN_DECLS
#include "core/gimpdrawableundo.h"
typedef struct _GimpMypaintCoreUndo GimpMypaintCoreUndo;
typedef struct _GimpMypaintCoreUndoClass GimpMypaintCoreUndoClass;
#define GIMP_TYPE_MYPAINT_CORE_UNDO (gimp_mypaint_core_undo_get_type ())
#define GIMP_MYPAINT_CORE_UNDO(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GIMP_TYPE_MYPAINT_CORE_UNDO, GimpMypaintCoreUndo))
#define GIMP_MYPAINT_CORE_UNDO_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GIMP_TYPE_MYPAINT_CORE_UNDO, GimpMypaintCoreUndoClass))
#define GIMP_IS_MYPAINT_CORE_UNDO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GIMP_TYPE_MYPAINT_CORE_UNDO))
#define GIMP_IS_MYPAINT_CORE_UNDO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GIMP_TYPE_MYPAINT_CORE_UNDO))
#define GIMP_MYPAINT_CORE_UNDO_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GIMP_TYPE_MYPAINT_CORE_UNDO, GimpMypaintCoreUndoClass))
/* Source-name compatibility only: native DrawableUndo parent replaces the old
 * raw Stroke pointer ABI. Instance storage is private, never a raw C++ object. */
GType gimp_mypaint_core_undo_get_type(void) G_GNUC_CONST;
G_END_DECLS
#endif
