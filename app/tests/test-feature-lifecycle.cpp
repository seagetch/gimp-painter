/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
extern "C" {
#include "core/core-types.h"
#include "widgets/widgets-types.h"
}
#include "presets/preset-factory-gui.hpp"
extern "C" void feature_handle_test (Gimp *application);
extern "C" void feature_handle_test (Gimp *application)
{
  auto owner = GimpPainter::ApplicationRef::retain (application);
  owner.initialize_features ();
}
