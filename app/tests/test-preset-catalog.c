/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <glib/gstdio.h>
#include <json-glib/json-glib.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "gegl/gimp-gegl.h"
#include "core/gimp-contexts.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpdataloaderfactory.h"
#include "core/gimpasync.h"
#include "core/gimpasyncset.h"
#include "core/gimpwaitable.h"
#include "core/gimplayerpreset.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpaction.h"
#include "presets/preset-factory-gui.h"
#include "presets/gimppresetdescriptor-private.h"
#include "gimp-features.h"
#include "gimp-log.h"
#include "tests.h"
#include "actions/dialogs-actions.h"
#include "dialogs/dialogs.h"
#include "gimp-app-test-utils.h"

/* Only coexistence cases share the native global unit service.  Each target
 * still constructs and destroys its own real descriptor, factory and Gimp.
 * The ordinary single-owner cases never activate these wrappers. */
static gboolean shared_units;
static Gimp *unit_guardian;
static guint unit_targets_opened, unit_targets_closed;
void __real_gimp_units_init (Gimp *);
void __real_gimp_units_exit (Gimp *);
void __wrap_gimp_units_init (Gimp *);
void __wrap_gimp_units_exit (Gimp *);
void __wrap_gimp_units_init (Gimp *g)
{
  if (!shared_units) { __real_gimp_units_init (g); return; }
  g_assert_nonnull (unit_guardian); g_assert_null (g->user_units);
  unit_targets_opened++; g_print ("unit boundary: target init bypass only\n");
}
void __wrap_gimp_units_exit (Gimp *g)
{
  if (!shared_units || g == unit_guardian)
    { __real_gimp_units_exit (g); return; }
  g_list_free_full (g->user_units, g_object_unref); g->user_units = NULL;
  unit_targets_closed++; g_print ("unit boundary: target list cleanup only\n");
}
static gpointer tracked_descriptors[64];
static guint tracked_frees[64], tracked_count;
void __real_g_free (gpointer);
void __wrap_g_free (gpointer);
void __wrap_g_free (gpointer pointer)
{
  guint i;
  for(i=0;i<tracked_count;i++)
    if(tracked_descriptors[i]==pointer && tracked_frees[i]==0)
      { tracked_frees[i]++; break; }
  __real_g_free(pointer);
}
static void track_descriptor (gpointer d)
{ g_assert_cmpuint(tracked_count,<,G_N_ELEMENTS(tracked_descriptors));tracked_descriptors[tracked_count++]=d; }
static gboolean fail_alloc, suppress_actions, close_on_swap;
static guint prepare_mode, swap_calls;
static Gimp *other_owner, *swap_owner;
static GimpPresetDescriptor *nested_descriptor;
gpointer __real_g_try_malloc0 (gsize);
gpointer __wrap_g_try_malloc0 (gsize);
gpointer __wrap_g_try_malloc0 (gsize n)
{ if (fail_alloc) { fail_alloc = FALSE; return NULL; } return __real_g_try_malloc0 (n); }
GimpDataFactory *__real_gimp_data_loader_factory_new (Gimp *, GType, const gchar *, const gchar *, const gchar *, GimpDataNewFunc, GimpDataGetStandardFunc);
GimpDataFactory *__wrap_gimp_data_loader_factory_new (Gimp *, GType, const gchar *, const gchar *, const gchar *, GimpDataNewFunc, GimpDataGetStandardFunc);
GimpDataFactory *__wrap_gimp_data_loader_factory_new (Gimp *g, GType t, const gchar *p, const gchar *w, const gchar *e, GimpDataNewFunc n, GimpDataGetStandardFunc s)
{
  GimpDataFactory *result = __real_gimp_data_loader_factory_new (g,t,p,w,e,n,s);
  guint mode;
  if (t != GIMP_TYPE_LAYER_PRESET) return result;
  mode = prepare_mode; prepare_mode = 0;
  if (mode == 1) gimp_features_close (g);
  if (mode == 2)
    { GError *error = NULL; g_assert_true (gimp_preset_catalog_register (other_owner, nested_descriptor, &error)); g_assert_no_error (error); }
  if (mode == 3) { g_object_unref (result); return NULL; }
  if (mode == 4)
    { g_assert_null (gimp_preset_catalog_snapshot (g)); g_assert_null (g->layer_preset_factory); }
  return result;
}
void __real_gimp_action_group_add_string_actions (GimpActionGroup *, const gchar *, const GimpStringActionEntry *, guint, GCallback);
void __wrap_gimp_action_group_add_string_actions (GimpActionGroup *, const gchar *, const GimpStringActionEntry *, guint, GCallback);
void __wrap_gimp_action_group_add_string_actions (GimpActionGroup *g, const gchar *c, const GimpStringActionEntry *e, guint n, GCallback cb)
{ if (!suppress_actions) __real_gimp_action_group_add_string_actions (g,c,e,n,cb); }
static Gimp *cold (void)
{
  Gimp *g = gimp_new ("Preset catalog", NULL, NULL, FALSE, TRUE, TRUE, TRUE,
    FALSE, FALSE, TRUE, FALSE, FALSE, GIMP_STACK_TRACE_NEVER, GIMP_PDB_COMPAT_OFF);
  g_assert_nonnull (g->layer_preset_factory);
  {
    GFileIOStream *stream; GError *error = NULL;
    GFile *rc = g_file_new_tmp ("preset-catalog-rc-XXXXXX", &stream, &error);
    gboolean oncanvas = FALSE;
    g_assert_no_error (error); g_assert_true (g_io_stream_close (G_IO_STREAM(stream),NULL,&error));
    g_object_unref(stream); gimp_load_config(g,NULL,rc);
    gimp_rc_set_autosave(GIMP_RC(g->config),FALSE);gimp_rc_set_autosave(GIMP_RC(g->edit_config),FALSE);
    g_object_set(g->config,"painter-canvas-ui",TRUE,NULL);g_object_get(g->config,"painter-canvas-ui",&oncanvas,NULL);g_assert_true(oncanvas);
    g_assert_true(g_file_delete(rc,NULL,&error));g_assert_no_error(error);g_object_unref(rc);
  }
  return g;
}
static void destroy (Gimp *g)
{ gimp_contexts_exit (g); g_object_unref (g); }
static GimpPresetDescriptor *descriptor (const gchar *name, gboolean alternate)
{
  GimpPresetDescriptor *base = gimp_preset_catalog_snapshot (NULL), *d;
  GimpPresetDescriptorValues v = *gimp_preset_descriptor_values (base);
  GError *error = NULL;
  v.tree_label = v.label = v.dialog_label = v.action_label = name;
  if (alternate) { v.path_property_name = "tool-preset-path"; v.writable_property_name = "tool-preset-path-writable"; }
  else { v.path_property_name = "layer-presets-path"; v.writable_property_name = "layer-presets-path-writable"; }
  d = gimp_preset_descriptor_new (&v, &error); g_assert_no_error (error); g_assert_nonnull (d);
  gimp_preset_descriptor_unref (base); return d;
}
static void install (Gimp *g, GimpPresetDescriptor *d)
{ GError *e = NULL; g_assert_true (gimp_preset_catalog_register (g,d,&e)); g_assert_no_error (e); }
static void rejects (Gimp *g, GimpPresetDescriptor *d)
{ GError *e = NULL; g_assert_false (gimp_preset_catalog_register (g,d,&e)); g_assert_nonnull (e); g_clear_error (&e); }
static void assert_pair (Gimp *g, const gchar *name, const gchar *path)
{
  GimpPresetDescriptor *d = gimp_preset_catalog_snapshot (g);
  gchar *actual = NULL;
  g_assert_cmpstr (gimp_preset_descriptor_values (d)->label, ==, name);
  g_assert_cmpstr (gimp_preset_descriptor_values (d)->path_property_name, ==, path);
  g_object_get (g->layer_preset_factory, "path-property-name", &actual, NULL);
  g_assert_cmpstr (actual, ==, path); g_free (actual);
  g_assert_true (gimp_get_data_factory (g, GIMP_TYPE_LAYER_PRESET) == g->layer_preset_factory);
  gimp_preset_descriptor_unref (d);
}
static void swap_observer (gpointer data, GObject *old, gboolean last)
{
  GimpPresetDescriptor *d;
  (void)data;
  if (!last) return;
  swap_calls++;
  g_assert_true (G_OBJECT (swap_owner->layer_preset_factory) != old);
  assert_pair (swap_owner, "replacement", "tool-preset-path");
  d = gimp_preset_catalog_snapshot (NULL);
  g_assert_cmpstr (gimp_preset_descriptor_values (d)->label, ==, "replacement");
  gimp_preset_descriptor_unref (d);
  rejects (swap_owner, nested_descriptor);
  if (close_on_swap) gimp_features_close (swap_owner);
}
static void acquisition_observer (gpointer data, GObject *object, gboolean last)
{ if (!last) gimp_features_close (GIMP (object)); (void)data; }
static void status_dummy (const gchar *a, const gchar *b, gdouble progress)
{ (void)a; (void)b; (void)progress; }
static void selected_persistence (Gimp *a, GimpPresetDescriptor *d)
{
  GError *error=NULL; gchar *dir=g_dir_make_tmp("preset-catalog-data-XXXXXX",&error);
  gchar *source=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"data","layer-presets","new-anime-layer.json",NULL);
  gchar *file=g_build_filename(dir,"catalog.json",NULL), *text=NULL;gsize size=0;
  GimpContainer *container;GimpLayerPreset *preset;JsonNode *root;JsonParser *parser;
  g_assert_no_error(error);g_assert_true(g_file_get_contents(source,&text,&size,&error));
  g_assert_true(g_file_set_contents(file,text,size,&error));g_free(text);g_assert_no_error(error);
  g_object_set(a->config,"tool-preset-path",dir,"tool-preset-path-writable",dir,
    "layer-presets-path","/nonexistent/catalog-unselected",NULL);
  install(a,d);gimp_preset_owner_freeze(a);
  gimp_data_factory_data_init(a->layer_preset_factory,a->user_context,FALSE);
  container=gimp_data_factory_get_container(a->layer_preset_factory);
  g_assert_cmpint(gimp_container_get_n_children(container),==,1);
  preset=GIMP_LAYER_PRESET(gimp_container_get_first_child(container));root=gimp_layer_preset_dup_json(preset);
  json_object_set_string_member(json_node_get_object(root),"catalog-proof","selected-alternate-path");
  g_assert_true(gimp_layer_preset_set_json(preset,root,&error));g_assert_no_error(error);json_node_unref(root);
  gimp_data_dirty(GIMP_DATA(preset));gimp_data_factory_data_save(a->layer_preset_factory);
  parser=json_parser_new();g_assert_true(json_parser_load_from_file(parser,file,&error));g_assert_no_error(error);
  g_assert_cmpstr(json_object_get_string_member(json_node_get_object(json_parser_get_root(parser)),"catalog-proof"),==,"selected-alternate-path");g_object_unref(parser);
  gimp_data_factory_data_free(a->layer_preset_factory);
  gimp_data_factory_data_refresh(a->layer_preset_factory,a->user_context);
  g_assert_cmpint(gimp_container_get_n_children(container),==,1);
  root=gimp_layer_preset_dup_json(GIMP_LAYER_PRESET(gimp_container_get_first_child(container)));
  g_assert_cmpstr(json_object_get_string_member(json_node_get_object(root),"catalog-proof"),==,"selected-alternate-path");json_node_unref(root);
  g_assert_cmpint(g_remove(file),==,0);g_assert_cmpint(g_rmdir(dir),==,0);g_free(file);g_free(source);g_free(dir);
}
static void run (gconstpointer value)
{
  guint id = GPOINTER_TO_UINT (value);
  Gimp *a, *b = NULL;
  GimpPresetDescriptor *d, *e;
  GimpDataFactory *old;
  if (!g_test_subprocess ())
    { g_test_trap_subprocess (NULL, 60 * G_TIME_SPAN_SECOND, 0);
      if (id == 22) { g_test_trap_assert_failed (); g_test_trap_assert_stderr ("*Cannot initialize required preset dialog descriptor*"); }
      else g_test_trap_assert_passed ();
      if (id == 2 || id == 3 || id == 8 || id == 12 || id == 13)
        {
          g_test_trap_assert_stdout ("*unit boundary: target init bypass only*unit boundary: target list cleanup only*unit boundary: guardian real teardown last*targets*");
          g_test_message ("verified unit-service-only wrapper trace; catalog/factory lifecycle unwrapped");
        }
      return; }
  g_setenv ("GIMP_PAINTER_HTTP_ENABLE", "0", TRUE);
  d = descriptor ("replacement", TRUE); e = descriptor ("second", FALSE);
  if (id == 0)
    {
      GArray *before, *after;
      GimpPresetPreferencesSnapshot *retained;
      install (NULL,d); before = preset_factory_gui_prefs_entry_point ();
      retained=gimp_preset_preferences_snapshot_new(NULL,NULL);
      a = cold (); after = preset_factory_gui_prefs_entry_point ();
      g_assert_cmpstr (g_array_index (before,PrefsDialogEntry,0).label, ==, "replacement");
      g_assert_cmpstr (g_array_index (after,PrefsDialogEntry,0).label, ==, "Layer presets Folders");
      g_array_unref (after); destroy (a);
      g_assert_cmpstr (gimp_preset_preferences_snapshot_get_entries(retained,NULL)->path_property_name, ==, "tool-preset-path");
      gimp_preset_preferences_snapshot_unref(retained);
      g_array_unref (before); goto done;
    }
  if (id == 2 || id == 3 || id == 8 || id == 12 || id == 13)
    {
      unit_guardian = cold ();
      gimp_features_close (unit_guardian); /* No catalog owner remains. */
      shared_units = TRUE;
    }
  if (id == 15) prepare_mode = 4;
  a = cold (); old = a->layer_preset_factory;
  if (id == 1)
    {
      GimpPresetDescriptor *retained;
      GArray *array;
      GimpDataFactory *old_ref = g_object_ref (old);
      install (a,d); retained = gimp_preset_catalog_snapshot (a); array = preset_factory_gui_prefs_entry_point ();
      assert_pair (a,"replacement","tool-preset-path");
      g_assert_true (a->layer_preset_factory != old_ref);
      g_assert_cmpint (gimp_container_get_n_children (gimp_data_factory_get_container (old_ref)), ==, 0);
      install (a,e); assert_pair (a,"second","layer-presets-path");
      g_assert_cmpstr (gimp_preset_descriptor_values (retained)->label, ==, "replacement");
      g_assert_cmpstr (g_array_index (array,PrefsDialogEntry,0).label, ==, "replacement");
      g_object_unref (old_ref); g_array_unref (array); gimp_preset_descriptor_unref (retained);
    }
  else if (id == 2 || id == 3)
    {
      b = cold (); install (a,d); install (b,e);
      assert_pair (a,"replacement","tool-preset-path"); assert_pair (b,"second","layer-presets-path");
      gimp_preset_owner_freeze (id == 2 ? a : b); rejects (a,e); rejects (b,d);
      destroy (id == 2 ? a : b);
      if (id == 2) { a = b; b = NULL; } else b = NULL;
      install (a,d); assert_pair (a,"replacement","tool-preset-path");
    }
  else if (id == 4)
    {
      GimpPresetDescriptorValues v = *gimp_preset_descriptor_values (d);
      GimpPresetDescriptor *invalid; GError *error = NULL;
      v.path_property_name = "verbose"; invalid = gimp_preset_descriptor_new (&v,&error);
      g_assert_no_error (error); rejects (a,invalid); gimp_preset_descriptor_unref (invalid);
      v.path_property_name = "missing-catalog-path"; invalid = gimp_preset_descriptor_new (&v,&error);
      rejects (a,invalid); gimp_preset_descriptor_unref (invalid);
      fail_alloc = TRUE; invalid = gimp_preset_descriptor_new (gimp_preset_descriptor_values (d),&error);
      g_assert_null (invalid); g_assert_nonnull (error); g_clear_error (&error);
      g_assert_true (a->layer_preset_factory == old);
      install (a,d);
    }
  else if (id == 5)
    {
      GimpData *data = gimp_layer_preset_new (a->user_context,"occupied");
      GimpContainer *container = gimp_data_factory_get_container (old);
      GimpAsync *async = gimp_async_new ();
      gimp_container_add (container,GIMP_OBJECT(data)); rejects (a,d);
      gimp_container_remove (container,GIMP_OBJECT(data)); g_object_unref(data);
      gimp_async_set_add (gimp_data_factory_get_async_set(old),async); rejects(a,d);
      gimp_async_finish(async,NULL); gimp_waitable_wait(GIMP_WAITABLE(async));
      g_assert_true(gimp_async_set_is_empty(gimp_data_factory_get_async_set(old)));
      g_object_unref(async); install(a,d);
    }
  else if (id == 6)
    {
      swap_owner = a; nested_descriptor = e;
      g_object_add_toggle_ref (G_OBJECT(old),swap_observer,NULL);
      install(a,d); g_assert_cmpuint(swap_calls,==,1);
      g_object_remove_toggle_ref(G_OBJECT(old),swap_observer,NULL);
    }
  else if (id == 7)
    { prepare_mode = 1; rejects(a,d); g_assert_true(a->layer_preset_factory==old); }
  else if (id == 8)
    {
      b=cold(); other_owner=b; nested_descriptor=e; prepare_mode=2;
      rejects(a,d); g_assert_true(a->layer_preset_factory==old);
      assert_pair(b,"second","layer-presets-path");
    }
  else if (id == 9)
    {
      g_object_add_toggle_ref(G_OBJECT(a),acquisition_observer,NULL); g_object_unref(a);
      rejects(a,d); g_assert_true(a->layer_preset_factory==old);
      g_object_ref(a); g_object_remove_toggle_ref(G_OBJECT(a),acquisition_observer,NULL);
    }
  else if (id == 10)
    {
      GimpPresetGuiLease *lease=gimp_preset_gui_begin(a);
      g_assert_nonnull(lease); rejects(a,d);
      g_assert_false(gimp_preset_gui_end(lease,FALSE)); install(a,d);
      lease=gimp_preset_gui_begin(a); g_assert_true(gimp_preset_gui_end(lease,TRUE)); rejects(a,e);
    }
  else if (id == 11)
    {
      GimpActionGroup *group = gimp_action_group_new(a,"catalog-actions","catalog",NULL,NULL,NULL);
      suppress_actions=TRUE; preset_factory_gui_dialogs_actions_entry_point(group);
      g_assert_null(gimp_action_group_get_action(group,"dialogs-layer-presets"));
      install(a,d); suppress_actions=FALSE; preset_factory_gui_dialogs_actions_entry_point(group);
      g_assert_cmpstr(gimp_action_get_label(gimp_action_group_get_action(group,"dialogs-layer-presets")),==,"replacement");
      rejects(a,e); g_object_unref(group);
    }
  else if (id == 12)
    {
      GimpPresetGuiLease *lease=gimp_preset_gui_begin(a);
      gimp_features_close(a); g_assert_false(gimp_preset_gui_end(lease,TRUE));
      b=cold(); install(b,d); destroy(b); b=NULL;
    }
  else if (id == 13)
    {
      GTypeQuery q; g_type_query(GIMP_TYPE_GIMP,&q);
      g_assert_cmpuint(q.instance_size,==,sizeof(Gimp));
      g_assert_cmpuint(q.class_size,==,sizeof(GimpClass));
      gimp_preset_owner_freeze(a); b=cold(); gimp_preset_owner_freeze(b);
      gimp_features_close(a); rejects(b,d); gimp_features_close(b);
      destroy(b); b=cold(); install(b,d);
    }
  else if (id == 14)
    { prepare_mode = 3; rejects (a,d); g_assert_true (a->layer_preset_factory == old); install(a,d); }
  else if (id == 15)
    { g_assert_cmpuint(prepare_mode,==,0); install(a,d); }
  else if (id == 23)
    {
      GArray *arrays[32];guint count;gsize bytes,previous=0;
      for(guint i=0;i<32;i++)
        {
          gchar *label=g_strdup_printf("borrowed epoch %u",i);
          GimpPresetDescriptor *next=descriptor(label,TRUE);GArray *duplicate;
          track_descriptor(next);install(a,next);arrays[i]=preset_factory_gui_prefs_entry_point();
          duplicate=preset_factory_gui_prefs_entry_point();g_array_unref(duplicate);
          gimp_preset_catalog_legacy_usage(&count,&bytes);g_assert_cmpuint(count,==,i+1);g_assert_cmpuint(bytes,>,previous);previous=bytes;
          gimp_preset_descriptor_unref(next);g_free(label);
        }
      for(guint i=0;i<32;i++)
        {
          gchar *expected=g_strdup_printf("borrowed epoch %u",i);
          g_assert_cmpstr(g_array_index(arrays[i],PrefsDialogEntry,0).label,==,expected);g_free(expected);
          g_array_index(arrays[i],PrefsDialogEntry,0).label="caller literal";
          g_array_unref(arrays[i]);g_assert_cmpuint(tracked_frees[i],==,0);
        }
      gimp_preset_catalog_legacy_usage(&count,&bytes);g_assert_cmpuint(count,==,32);
      destroy(a);a=NULL;gimp_preset_catalog_legacy_usage(&count,&bytes);
      g_assert_cmpuint(count,==,0);g_assert_cmpuint(bytes,==,0);
      for(guint i=0;i<32;i++)g_assert_cmpuint(tracked_frees[i],==,1);
    }
  else if (id == 24)
    {
      GimpPresetPreferencesSnapshot *snapshot,*copy;const PrefsDialogEntry *view;guint count;
      install(a,d);track_descriptor(d);snapshot=gimp_preset_preferences_snapshot_new(a,NULL);g_assert_nonnull(snapshot);
      copy=gimp_preset_preferences_snapshot_ref(snapshot);view=gimp_preset_preferences_snapshot_get_entries(copy,&count);g_assert_cmpuint(count,==,1);
      gimp_preset_descriptor_unref(d);d=NULL;install(a,e);destroy(a);a=NULL;
      g_assert_cmpuint(tracked_frees[0],==,0);g_assert_cmpstr(view->label,==,"replacement");
      gimp_preset_preferences_snapshot_unref(snapshot);g_assert_cmpstr(view->path_property_name,==,"tool-preset-path");
      gimp_preset_preferences_snapshot_unref(copy);g_assert_cmpuint(tracked_frees[0],==,1);
      g_assert_null(gimp_preset_preferences_snapshot_get_entries(NULL,&count));g_assert_cmpuint(count,==,0);
    }
  else if (id == 25)
    {
      GError *error=NULL;GimpPresetPreferencesSnapshot *snapshot;
      fail_alloc=TRUE;snapshot=gimp_preset_preferences_snapshot_new(a,&error);
      g_assert_null(snapshot);g_assert_nonnull(error);g_clear_error(&error);install(a,d);
    }
  else if (id == 20)
    {
      GimpActionGroup *group=gimp_action_group_new(a,"fallback-actions","Fallback",NULL,NULL,NULL);
      fail_alloc=TRUE;dialogs_actions_setup(group);
      g_assert_nonnull(gimp_action_group_get_action(group,"dialogs-tool-options"));
      g_assert_nonnull(gimp_action_group_get_action(group,"dialogs-preferences"));
      g_assert_null(gimp_action_group_get_action(group,"dialogs-layer-presets"));
      install(a,d);g_object_unref(group);
    }
  else if (id == 21)
    { fail_alloc=TRUE;g_assert_null(gimp_preset_gui_begin(a));install(a,d); }
  else if (id == 22)
    { fail_alloc=TRUE;dialogs_init(a);g_assert_not_reached(); }
  else if (id == 19)
    {
      GArray *array;PrefsDialogEntry *stolen;guint n;
      install(a,d);array=preset_factory_gui_prefs_entry_point();g_array_ref(array);g_array_unref(array);
      install(a,e);g_assert_cmpstr(g_array_index(array,PrefsDialogEntry,0).label,==,"replacement");
      n=array->len;stolen=(PrefsDialogEntry*)g_array_free(array,FALSE);g_assert_cmpuint(n,==,1);
      g_assert_cmpstr(stolen[0].label,==,"replacement");stolen[0].label="literal replacement remains caller-local";g_free(stolen);
      array=preset_factory_gui_prefs_entry_point();g_array_free(array,TRUE);
    }
  else if (id == 16) selected_persistence(a,d);
  else if (id == 17)
    { install(a,d);gimp_gegl_init(a);gimp_initialize(a,status_dummy);rejects(a,e);assert_pair(a,"replacement","tool-preset-path"); }
  else if (id == 18)
    {
      swap_owner=a;nested_descriptor=e;close_on_swap=TRUE;
      g_object_add_toggle_ref(G_OBJECT(old),swap_observer,NULL);
      install(a,d);g_assert_false(gimp_preset_owner(a)->active);
      g_object_remove_toggle_ref(G_OBJECT(old),swap_observer,NULL);
    }
  if(b)destroy(b);
  if(a)destroy(a);
done:
  if (unit_guardian)
    {
      g_assert_cmpuint (unit_targets_opened, ==, unit_targets_closed);
      g_assert_cmpuint (unit_targets_opened, >=, 2);
      g_print ("unit boundary: guardian real teardown last; %u targets\n", unit_targets_closed);
      destroy (unit_guardian); unit_guardian = NULL; shared_units = FALSE;
    }
  gimp_preset_descriptor_unref(d);gimp_preset_descriptor_unref(e);
  { GimpPresetDescriptor *reset=gimp_preset_catalog_snapshot(NULL);g_assert_cmpstr(gimp_preset_descriptor_values(reset)->label,==,"Layer presets Folders");gimp_preset_descriptor_unref(reset); }
}
int main(int argc,char **argv)
{
  const gchar *names[]={"preentry-overwrite-array","replace-retained-snapshots","two-owners-first-close","two-owners-second-close","invalid-allocation-rollback","nonempty-active-load","coherent-release-reentry","prepare-owner-close","prepare-other-owner-generation","acquisition-close","gui-cancel-publish","actual-action-empty-publish","gui-close-release","private-size-double-close","factory-preparation-null","startup-absent-pair","selected-real-load-save-reopen","actual-initialize-freeze","release-then-owner-close","array-ref-free-steal","unrelated-actions-on-lease-failure","failed-gui-allocation-no-freeze","required-dialog-startup-failure","legacy-epoch-dedup-release32","owning-snapshot-survives-close","owning-snapshot-allocation-failure"};
  g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_log_init();gegl_init(NULL,NULL);
  gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");
  for(guint i=0;i<G_N_ELEMENTS(names);i++){gchar *path=g_strconcat("/preset-catalog/",names[i],NULL);g_test_add_data_func(path,GUINT_TO_POINTER(i),run);g_free(path);}
  return g_test_run();
}
