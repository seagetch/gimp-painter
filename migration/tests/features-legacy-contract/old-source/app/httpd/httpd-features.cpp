/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * httpd-features
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
#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
};

#include "httpd-features.h"
#include "httpd.h"
#include "rest-pdb.h"
#include "rest-image-tree.h"
#include "navigation-guide.h"

///////////////////////////////////////////////////////////////////////////
// HTTPDFeature

class HTTPDFeature : public GIMP::Feature {
  CXXPointer<RESTD> rest_daemon;
public:
  HTTPDFeature() { }
  virtual void     initialize (Gimp* gimp, GimpInitStatusFunc callback);
  virtual void     restore    (Gimp* gimp, GimpInitStatusFunc callback);
  virtual gboolean exit       (Gimp *gimp, gboolean force);
};

static HTTPDFeature* singleton = NULL;

GIMP::Feature* httpd_get_factory()
{
  if (!singleton)
    singleton = new HTTPDFeature();
  return singleton;
}

void
HTTPDFeature::initialize (Gimp* gimp, GimpInitStatusFunc callback)
{
  g_print(">HTTPDFeature::initialize\n");
  rest_daemon = new RESTD(gimp);
  rest_daemon->route("/<name>", RESTD::delegator([&](auto matched, auto gimp, auto msg, auto context) {
    auto data = matched->data();
    const gchar* name = data.lookup("name");
    GLib::CString text = g_strdup_printf("Hello, %s", name);
    g_print("text=%s\n", (const gchar*)text);
    soup_message_set_response (msg,
                               "text/plain",
                               SOUP_MEMORY_COPY,
                               text,
                               strlen(text));
  }));
  auto pdb_factory              = new RESTPDBFactory;
  auto image_tree_factory       = new RESTImageTreeFactory;
  rest_daemon->route("/api/v1/pdb/", pdb_factory);
  rest_daemon->route("/api/v1/pdb/{name}", pdb_factory);
  rest_daemon->route("/api/v1/images/**", image_tree_factory);
  rest_daemon->run();
  g_print("<HTTPDFeature::initialize\n");
}

void
HTTPDFeature::restore    (Gimp* gimp, GimpInitStatusFunc callback)
{
}

gboolean
HTTPDFeature::exit       (Gimp *gimp, gboolean force)
{
  return FALSE;
}
