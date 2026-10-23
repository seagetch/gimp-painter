/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpdataloaderfactory.h"
#include "core/gimplayerpreset.h"
#include "painter/gimp-painter-error.h"
#include "gimp-features.h"
#ifdef HAVE_PAINTER_HTTP
#include "httpd/httpd.h"
#endif

enum { FEATURES_UNREGISTERED, FEATURES_REGISTERED, FEATURES_INITIALIZED,
       FEATURES_RESTORING, FEATURES_RESTORED, FEATURES_FAILED, FEATURES_CLOSED };

void
features_entry_point (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));
  if (gimp->painter_features_phase != FEATURES_UNREGISTERED) return;
  /* Publish registration before constructing the sole native resource owner. */
  gimp->painter_features_phase = FEATURES_REGISTERED;
  gimp->layer_preset_factory =
    gimp_data_loader_factory_new (gimp, GIMP_TYPE_LAYER_PRESET,
                                  "layer-presets-path", "layer-presets-path-writable",
                                  "layer-preset-paths", NULL, NULL);
  gimp_object_set_static_name (GIMP_OBJECT (gimp->layer_preset_factory),
                               "layer preset factory");
  gimp_data_loader_factory_add_loader (gimp->layer_preset_factory,
                                       "Painter Layer Preset", gimp_layer_preset_load,
                                       ".json", TRUE);
}

void
gimp_features_initialized (Gimp *gimp)
{
  g_return_if_fail (GIMP_IS_GIMP (gimp));
  if (gimp->painter_features_phase == FEATURES_REGISTERED)
    gimp->painter_features_phase = FEATURES_INITIALIZED;
}

gboolean
gimp_features_restored (Gimp *gimp, GError **error)
{
  GObject *service = NULL;
  gboolean success = TRUE;
  GError *local_error = NULL;

  g_return_val_if_fail (GIMP_IS_GIMP (gimp), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  if (gimp->painter_features_phase == FEATURES_RESTORED) return TRUE;
  if (gimp->painter_features_phase == FEATURES_FAILED)
    {
      g_set_error_literal (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_INVALID_STATE,
                           "Painter feature startup previously failed");
      return FALSE;
    }
  if (gimp->painter_features_phase != FEATURES_INITIALIZED || !gimp->restored)
    {
      g_set_error_literal (error, GIMP_PAINTER_ERROR,
                           gimp->painter_features_phase == FEATURES_CLOSED ?
                           GIMP_PAINTER_ERROR_CLOSED : GIMP_PAINTER_ERROR_INVALID_STATE,
                           "Painter features are not ready for restore");
      return FALSE;
    }
  g_object_ref (gimp);
  gimp->painter_features_phase = FEATURES_RESTORING;
#ifdef HAVE_PAINTER_HTTP
  service = gimp_painter_httpd_from_environment (gimp, &local_error);
  success = local_error == NULL;
#endif
  if (!success && service)
    {
      g_object_run_dispose (service);
      g_clear_object (&service);
    }
  /* Disposal during provider callbacks must never republish a service. */
  if (gimp->painter_features_phase == FEATURES_CLOSED)
    {
      if (service) g_object_run_dispose (service);
      g_clear_object (&service);
      g_clear_error (&local_error);
      g_set_error_literal (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED,
                           "Painter features closed during restore");
      g_object_unref (gimp);
      return FALSE;
    }
  gimp->painter_features_phase = success ? FEATURES_RESTORED : FEATURES_FAILED;
  if (service)
    {
      gimp->painter_httpd = g_object_ref (service);
      g_object_unref (service);
    }
  success = success && gimp->painter_features_phase != FEATURES_CLOSED;
  if (local_error) g_propagate_error (error, local_error);
  g_object_unref (gimp);
  return success;
}

void
gimp_features_close (Gimp *gimp)
{
  GObject *service;
  g_return_if_fail (GIMP_IS_GIMP (gimp));
  if (gimp->painter_features_phase == FEATURES_CLOSED) return;
  g_object_ref (gimp);
  gimp->painter_features_phase = FEATURES_CLOSED;
  service = gimp->painter_httpd;
  gimp->painter_httpd = NULL;
  if (service)
    {
      g_object_run_dispose (service);
      g_object_unref (service);
    }
  g_object_unref (gimp);
}
