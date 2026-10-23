/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "core/core-types.h"
extern "C" GimpUndo *gimp_image_undo_push_prepared(GimpImage*,GimpUndo*,GError**);
#include "core/gimpimage-undo.h"
#include "core/gimpimage-undo.h"
extern "C" void prepared_undo_cpp_signature(void);
extern "C" void prepared_undo_cpp_signature(void){GimpUndo*(*entry)(GimpImage*,GimpUndo*,GError**)=gimp_image_undo_push_prepared;g_assert_nonnull(entry);}
