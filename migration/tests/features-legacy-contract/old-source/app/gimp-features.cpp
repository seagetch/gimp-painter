/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Features
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

extern "C" {
#include "config.h"
#include "gimp-intl.h"
}

#include "base/glib-cxx-utils.hpp"
#include "gimp-features.h"


#ifndef GIMP_CONSOLE_COMPILATION
#include "presets/preset-factory-gui.h"
#else
#include "presets/preset-factory.h"
#endif

#ifdef USE_HTTPD
#ifdef GIMP_CONSOLE_COMPILATION
#include "httpd/httpd-features.h"
#else
#include "httpd/httpd-features-gui.h"
#endif
#endif


////////////////////////////////////////////////////////////////////////////
// Feature

typedef GIMP::Feature* (*feature_singleton_factory)(void);

feature_singleton_factory factory[] = {
#ifdef GIMP_CONSOLE_COMPILATION
    PresetFactory::get_factory,
#else
    PresetGuiFactory::get_factory,
#endif

#ifdef USE_HTTPD
  #ifdef GIMP_CONSOLE_COMPILATION
    httpd_get_factory,
  #else
    httpd_get_gui_factory,
  #endif
#endif
  NULL
};

static GArray* features = NULL;


void features_entry_point(Gimp* gimp)
{
  if (! features)
    features = g_array_new(TRUE, TRUE, sizeof(GIMP::Feature*));
  auto features_ = GLib::ref<GIMP::Feature*>(features);
  
  feature_singleton_factory* i = factory;
  for (; *i; i ++) {
    GIMP::Feature* feature = (*i)();
    features_.append(feature);
    feature->entry_point(gimp);
  }
}

namespace GIMP {

void 
Feature::entry_point(Gimp* gimp)
{
  g_signal_connect_delegator (G_OBJECT(gimp), "initialize", Delegators::delegator(this, &Feature::on_initialize));
  if (!lazy) {
    g_print("Feature::entry_point: add restore/exit on entry_point.\n");
    g_signal_connect_delegator (G_OBJECT(gimp), "restore", Delegators::delegator(this, &Feature::on_restore));
    g_signal_connect_delegator (G_OBJECT(gimp), "exit",    Delegators::delegator(this, &Feature::on_exit));
  }
}


void
Feature::on_initialize (Gimp               *gimp,
                        GimpInitStatusFunc  status_callback)
{
  initialize(gimp, status_callback);

  // Dirty hack: Registering restore and exit at last in order to
  // ensure that on_restore is called after dialog-restore
  // function is called.
  if (lazy) {
    g_print("Feature::entry_point: add restore/exit on initialize.\n");
    g_signal_connect_delegator (G_OBJECT(gimp), "restore", Delegators::delegator(this, &Feature::on_restore));
    g_signal_connect_delegator (G_OBJECT(gimp), "exit",    Delegators::delegator(this, &Feature::on_exit));
  }
}


void
Feature::on_restore (Gimp               *gimp,
                     GimpInitStatusFunc  status_callback)
{
  restore(gimp, status_callback);
}


gboolean
Feature::on_exit (Gimp               *gimp,
                  gboolean            force)
{
  return exit(gimp, force);
}

}; // namespace Gimp
