/* Private causal fixture: real Xvfb GDK devices; synthetic name and seat events.
 * No physical-device identity or tablet compatibility claim. */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpobject.h"
#include "core/gimplist.h"
#include "core/gimpcontext.h"
#include "core/gimptooloptions.h"
#include "core/gimptoolpreset.h"
#include "widgets/gimpdeviceinfo.h"
#include "widgets/gimpdevicemanager.h"
#include "widgets/gimpdevices.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"

static Gimp *gimp;
static GdkDevice *alias_devices[2];
static const gchar alias_name[] = "Painter synthetic duplicate exposure";
const gchar *__real_gdk_device_get_name (GdkDevice *device);
const gchar *__wrap_gdk_device_get_name (GdkDevice *device)
{
  if (device && (device == alias_devices[0] || device == alias_devices[1]))
    return alias_name;
  return __real_gdk_device_get_name (device);
}

static void
unique_name_restore (void)
{
  const gchar *first = "(GimpDeviceInfo \"Synthetic persisted profile\" (mode disabled))";
  const gchar *second = "(GimpDeviceInfo \"Synthetic persisted profile\" (mode screen))";
  for (guint unique = 0; unique <= 1; unique++)
    {
      GError *error = NULL;
      GimpContainer *copy = gimp_list_new (GIMP_TYPE_DEVICE_INFO, unique);
      GimpObject *original;
      g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (copy), first,
                                                    -1, gimp, &error));
      g_assert_no_error (error);
      original = gimp_container_get_child_by_name (copy, "Synthetic persisted profile");
      g_assert_nonnull (original);
      g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (copy), second,
                                                    -1, gimp, &error));
      g_assert_no_error (error);
      g_assert_cmpint (gimp_container_get_n_children (copy), ==, unique ? 1 : 2);
      if (unique)
        {
          g_assert_true (original == gimp_container_get_child_by_name (
                                      copy, "Synthetic persisted profile"));
          g_assert_cmpint (gimp_device_info_get_mode (GIMP_DEVICE_INFO (original)),
                           ==, GDK_MODE_SCREEN);
        }
      else
        g_assert_cmpint (gimp_device_info_get_mode (GIMP_DEVICE_INFO (original)),
                         ==, GDK_MODE_DISABLED);
      g_object_unref (copy);
    }
}

static void
superseded_remove (void)
{
  GdkDisplay *display = gdk_display_get_default ();
  GdkSeat *seat = gdk_display_get_default_seat (display);
  GList *devices = gdk_seat_get_slaves (seat, GDK_SEAT_CAPABILITY_ALL);
  GimpDeviceManager *manager = gimp_devices_get_manager (gimp);
  GimpDeviceInfo *info;
  GdkDevice *attached, *superseded;
  GimpToolOptions *options;
  guint found = 0;

  g_assert_false (gimp_has_focused_once (gimp));
  for (GList *iter = devices; iter && found < 2; iter = iter->next)
    if (gdk_device_get_source (iter->data) == GDK_SOURCE_MOUSE)
      alias_devices[found++] = g_object_ref (iter->data);
  g_list_free (devices);
  /* Missing two real devices is an environment failure, not a passing skip. */
  g_assert_cmpuint (found, ==, 2);
  g_assert_true (alias_devices[0] != alias_devices[1]);
  gimp_set_focused_once (gimp);
  g_assert_true (gimp_container_get_unique_names (GIMP_CONTAINER (manager)));
  info = GIMP_DEVICE_INFO (gimp_container_get_child_by_name (
                             GIMP_CONTAINER (manager), alias_name));
  g_assert_nonnull (info);
  attached = gimp_device_info_get_device (info, NULL);
  g_assert_true (attached == alias_devices[0] || attached == alias_devices[1]);
  superseded = attached == alias_devices[0] ? alias_devices[1] : alias_devices[0];
#ifndef G_OS_WIN32
  /* Documented native duplicate exposure policy: latest attachment replaces old. */
  g_assert_null (gimp_device_info_get_by_device (superseded));
  g_assert_true (gimp_device_info_get_by_device (attached) == info);
#endif
  options = GIMP_TOOL_PRESET (info)->tool_options;
  g_assert_nonnull (options);
  gimp_context_set_opacity (GIMP_CONTEXT (options), 0.37);

  /* Emit only the signal; never remove a real host/X server device.  The old
   * exposure can report removal after a replacement already owns its info. */
  g_signal_emit_by_name (seat, "device-removed", superseded);
  g_assert_true (gimp_device_info_get_device (info, NULL) == attached);
  g_assert_true (gimp_device_info_get_by_device (attached) == info);
  g_assert_cmpstr (gimp_object_get_name (info), ==, alias_name);
  g_assert_cmpfloat (gimp_context_get_opacity (GIMP_CONTEXT (options)), ==, 0.37);
}

int
main (int argc, char **argv)
{
  gint result;
  g_test_init (&argc, &argv, NULL);
  if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_test_add_func ("/device-name/unique-name-restore", unique_name_restore);
  g_test_add_func ("/device-name/superseded-removal", superseded_remove);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  g_clear_object (&alias_devices[0]);
  g_clear_object (&alias_devices[1]);
  return result;
}
