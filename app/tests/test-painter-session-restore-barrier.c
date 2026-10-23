/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native startup/session interruption tests.  Every case starts a fresh Gimp
 * before the first dock-fill idle; no unconditional initial settle is allowed.
 * The saved single-window fixture contains real embedded docks as well as a
 * legitimate external dock.  Child stderr remains visible to the strict outer
 * diagnostic classifier. */
#include "config.h"

#include <string.h>
#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "config/gimpcoreconfig.h"
#include "config/gimpguiconfig.h"
#include "core/gimp.h"
#include "core/gimp-gui.h"
#include "core/gimpcontainer.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpimagewindow.h"
#include "display/gimppaintercanvasui.h"
#include "gui/session.h"
#include "tools/gimp-tools.h"
#include "widgets/gimpdialogfactory.h"
#include "widgets/gimpdock.h"
#include "widgets/gimpdockable.h"
#include "widgets/gimpdockbook.h"
#include "widgets/gimpdockcolumns.h"
#include "widgets/gimpdockcontainer.h"
#include "widgets/gimpdockwindow.h"
#include "widgets/gimpsessioninfo.h"
#include "widgets/gimpsessioninfo-private.h"
#include "widgets/gimpsessioninfo-dock.h"
#include "widgets/gimpsessioninfo-book.h"
#include "widgets/gimpsessioninfo-dockable.h"
#include "widgets/gimpsessioninfo-aux.h"
#include "widgets/gimpsessionmanaged.h"
#include "widgets/gimptooloptionseditor.h"
#include "widgets/gimpwidgets-utils.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"

#define ENV_PROFILE "GIMP_TEST_RESTORE_PROFILE"
#define ENV_SINGLE  "GIMP_TEST_RESTORE_SINGLE"
#define ENV_PAINTER "GIMP_TEST_RESTORE_PAINTER"
#define ENV_RESTORE "GIMP_TEST_RESTORE_ENABLED"
#define ENV_PASS    "GIMP_TEST_RESTORE_PASS"

/* The books have distinctive page order/current page, and dock 0 has two
 * vertically ordered books. These are compared structurally, not by totals. */
static const gchar *dock_bodies[] = {
  "(book (current-page 0) (dockable \"gimp-tool-options\" (tab-style icon)))"
  "(book (position 190) (current-page 0)"
  " (dockable \"gimp-brush-grid\" (tab-style preview)))",
  "(book (current-page 1)"
  " (dockable \"gimp-layer-list\" (tab-style icon) (preview-size 32))"
  " (dockable \"gimp-channel-list\" (tab-style icon) (preview-size 32)))",
  "(book (current-page 1)"
  " (dockable \"gimp-pattern-grid\" (tab-style preview))"
  " (dockable \"gimp-gradient-list\" (tab-style preview)))",
  "(book (current-page 0) (dockable \"gimp-palette-editor\" (tab-style icon)))"
};
static const gchar *identifiers[] = {
  "gimp-tool-options", "gimp-brush-grid", "gimp-layer-list",
  "gimp-channel-list", "gimp-pattern-grid", "gimp-gradient-list",
  "gimp-palette-editor"
};
static const gchar *dock_signatures[] = {
  "[0:gimp-tool-options;][0:gimp-brush-grid;]",
  "[1:gimp-layer-list;gimp-channel-list;]",
  "[1:gimp-pattern-grid;gimp-gradient-list;]",
  "[0:gimp-palette-editor;]"
};

typedef enum
{
  REQUEST_CHANGE,
  REQUEST_LATEST,
  REQUEST_NET_ZERO,
  REQUEST_PAINTER_ONLY,
  REQUEST_NONE,
  REQUEST_UNCHANGED,
  REQUEST_MID_FILL,
  REQUEST_NESTED_FILL,
  REQUEST_SAVE_EARLY,
  REQUEST_SAVE_MID_FILL,
  REQUEST_SAVE_CLEAR,
  REQUEST_SAVE_ALWAYS,
  REQUEST_SAVE_THEN_CLEAR,
  REQUEST_QUIT_EARLY,
  REQUEST_QUIT_MID_FILL,
  REQUEST_DIRTY_CANCEL,
  REQUEST_REPEAT_BATCH,
  REQUEST_NO_RESTORE,
  REQUEST_ENUMERATION,
  REQUEST_DISPOSE_QUEUED,
  REQUEST_DISPOSE_ACTIVE,
  REQUEST_DISPOSE_CONSTRUCTOR,
  REQUEST_RESTORE_DURING_CONVERSION,
  REQUEST_RESTORE_DURING_CONVERSION_PENDING,
  REQUEST_ONE_IMAGE,
  REQUEST_TWO_IMAGES
} Request;

typedef struct
{
  const gchar *name;
  Request      request;
  gboolean     single;
  gboolean     painter;
  gboolean     final_single;
  gboolean     final_painter;
  gboolean     fresh_reader;
} RestoreCase;

typedef struct
{
  gpointer object;
  guint    destroyed;
  guint    finalized;
} Lifetime;

typedef struct
{
  GimpSessionInfo *info;
  GtkWidget       *widget;
  GList           *payload;
  Lifetime        *life;
  Lifetime        *info_life;
} Target;

typedef struct
{
  GtkWidget *widget;
  GtkWidget *parent;
  Lifetime  *life;
} Identity;

typedef struct
{
  const RestoreCase *test;
  GimpDialogFactory *factory;
  Lifetime         *factory_life;
  GPtrArray         *targets;
  GPtrArray         *identities;
  GPtrArray         *gui_widgets;
  GPtrArray         *gui_parents;
  GimpToolInfo      *tool;
  GtkWidget        *gui;
  GtkWidget        *editor;
  gchar            *original_file;
  gboolean          ready_seen;
  gboolean          interrupt_seen;
  gboolean          callback_active;
  gboolean          nested_peers_completed;
  gboolean          extra_dock;
  gboolean          conversion_interrupted;
  GimpSessionInfo   *recent_info;
  GtkWidget        *recent_page;
  Lifetime         *recent_life;
  Lifetime         *recent_target_life;
  guint             pending_transfer_removals;
  guint             fills_seen;
  guint             ready_notifications;
  guint             exit_requests;
  gboolean          last_exit_force;
  GtkWidget        *first_page;
  Lifetime         *first_page_life;
  gulong            pending_handler;
} Fixture;

static Gimp    *gimp;
static Fixture *fixture;

static void assert_contents (Fixture *f);
static void assert_final_graph (gboolean painter, gboolean single);

static void
settle (void)
{
  /* The deadline detects a stuck implementation; readiness is the evidence. */
  gint64 deadline = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;

  while (! gimp_test_gui_has_exited (gimp) && ! session_is_ready (gimp) &&
         g_get_monotonic_time () < deadline)
    g_main_context_iteration (NULL, FALSE);
  if (! gimp_test_gui_has_exited (gimp))
    g_assert_true (session_is_ready (gimp));
  gimp_test_run_mainloop_until_idle ();
  for (guint i = 0; i < 1000 && g_main_context_pending (NULL); i++)
    g_main_context_iteration (NULL, FALSE);
}

static void
lifetime_destroy (GtkWidget *widget,
                  Lifetime  *life)
{
  (void) widget;
  life->destroyed++;
}

static void
lifetime_finalize (gpointer data)
{
  ((Lifetime *) data)->finalized++;
}

static Lifetime *
watch (GObject *object)
{
  static GQuark key;
  Lifetime *life;

  if (! key)
    key = g_quark_from_static_string ("restore-barrier-test-lifetime");
  life = g_object_get_qdata (object, key);
  if (life)
    return life;
  /* Witnesses outlive the objects, including final harness shutdown. */
  life = g_new0 (Lifetime, 1);
  life->object = object;
  g_object_add_weak_pointer (object, &life->object);
  if (GTK_IS_WIDGET (object))
    g_signal_connect (object, "destroy", G_CALLBACK (lifetime_destroy), life);
  g_object_set_qdata_full (object, key, life, lifetime_finalize);
  return life;
}

static void
gather (GtkWidget *widget,
        GPtrArray *array)
{
  if (g_ptr_array_find (array, widget, NULL))
    return;
  g_ptr_array_add (array, widget);
  if (GTK_IS_CONTAINER (widget))
    {
      GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
      for (GList *iter = children; iter; iter = iter->next)
        gather (iter->data, array);
      g_list_free (children);
    }
}

static GPtrArray *
all_docks (GimpDialogFactory *factory)
{
  GPtrArray *result = g_ptr_array_new ();
  GList *windows = gimp_get_image_windows (gimp);
  GList *roots = g_list_copy (gimp_dialog_factory_get_open_dialogs (factory));

  roots = g_list_concat (roots, windows);
  for (GList *root = roots; root; root = root->next)
    if (GIMP_IS_DOCK_CONTAINER (root->data))
      {
        GList *docks = gimp_dock_container_get_docks (root->data);
        for (GList *iter = docks; iter; iter = iter->next)
          if (! g_ptr_array_find (result, iter->data, NULL))
            g_ptr_array_add (result, iter->data);
        g_list_free (docks);
      }
  g_list_free (roots);
  return result;
}

static const gchar *
dockable_identifier (GtkWidget *widget)
{
  GimpDialogFactoryEntry *entry = NULL;
  g_assert_nonnull (gimp_dialog_factory_from_widget (widget, &entry));
  g_assert_nonnull (entry);
  return entry->identifier;
}

static gchar *
dock_signature (GimpDock *dock)
{
  GString *result = g_string_new (NULL);

  for (GList *iter = gimp_dock_get_dockbooks (dock); iter; iter = iter->next)
    {
      GtkNotebook *book = iter->data;
      g_string_append_printf (result, "[%d:", gtk_notebook_get_current_page (book));
      for (gint i = 0; i < gtk_notebook_get_n_pages (book); i++)
        {
          GtkWidget *page = gtk_notebook_get_nth_page (book, i);
          g_assert_true (gimp_dockable_get_dockbook (GIMP_DOCKABLE (page)) ==
                         GIMP_DOCKBOOK (book));
          g_string_append_printf (result, "%s;", dockable_identifier (page));
        }
      g_string_append_c (result, ']');
    }
  return g_string_free (result, FALSE);
}

static void
assert_contents (Fixture *f)
{
  GPtrArray *docks = all_docks (f->factory);
  guint seen[G_N_ELEMENTS (dock_signatures)] = { 0 };
  guint pages = 0;

  g_assert_cmpuint (docks->len, ==, G_N_ELEMENTS (dock_signatures) + (f->extra_dock ? 1 : 0));
  for (guint i = 0; i < docks->len; i++)
    {
      GimpDock *dock = g_ptr_array_index (docks, i);
      gchar *signature = dock_signature (dock);
      gboolean found = FALSE;
      for (guint j = 0; j < G_N_ELEMENTS (dock_signatures); j++)
        if (g_str_equal (signature, dock_signatures[j]))
          {
            seen[j]++;
            found = TRUE;
          }
      if (f->extra_dock && g_str_equal (signature, "[0:gimp-error-console;]"))
        found = TRUE;
      g_assert_true (found);
      pages += gimp_dock_get_n_dockables (dock);
      g_free (signature);
    }
  for (guint i = 0; i < G_N_ELEMENTS (seen); i++)
    g_assert_cmpuint (seen[i], ==, 1);
  g_assert_cmpuint (pages, ==, G_N_ELEMENTS (identifiers) + (f->extra_dock ? 1 : 0));
  for (guint i = 0; i < G_N_ELEMENTS (identifiers); i++)
    {
      GtkWidget *page = gimp_dialog_factory_find_widget (f->factory, identifiers[i]);
      g_assert_true (GIMP_IS_DOCKABLE (page));
      g_assert_nonnull (gimp_dockable_get_dockbook (GIMP_DOCKABLE (page)));
    }
  g_ptr_array_unref (docks);
}

static void
remember_identity (Fixture   *f,
                   GtkWidget *widget)
{
  Identity *identity = g_new0 (Identity, 1);
  identity->widget = g_object_ref (widget);
  identity->parent = gtk_widget_get_parent (widget);
  identity->life = watch (G_OBJECT (widget));
  g_ptr_array_add (f->identities, identity);
}

static void
snapshot_native (Fixture *f)
{
  GPtrArray *docks = all_docks (f->factory);
  GtkWidget *dockable = gimp_dialog_factory_find_widget (f->factory, "gimp-tool-options");

  g_assert_cmpuint (f->identities->len, ==, 0);
  for (guint i = 0; i < docks->len; i++)
    {
      GimpDock *dock = g_ptr_array_index (docks, i);
      remember_identity (f, GTK_WIDGET (dock));
      for (GList *book = gimp_dock_get_dockbooks (dock); book; book = book->next)
        {
          GtkNotebook *notebook = book->data;
          remember_identity (f, GTK_WIDGET (notebook));
          for (gint page = 0; page < gtk_notebook_get_n_pages (notebook); page++)
            remember_identity (f, gtk_notebook_get_nth_page (notebook, page));
        }
    }
  f->editor = gtk_bin_get_child (GTK_BIN (dockable));
  g_assert_true (GIMP_IS_TOOL_OPTIONS_EDITOR (f->editor));
  remember_identity (f, f->editor);
  f->gui = gimp_tools_get_tool_options_gui (f->tool->tool_options);
  g_assert_nonnull (f->gui);
  f->gui_widgets = g_ptr_array_new ();
  f->gui_parents = g_ptr_array_new ();
  gather (f->gui, f->gui_widgets);
  g_assert_cmpuint (f->gui_widgets->len, >, 10);
  for (guint i = 0; i < f->gui_widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (f->gui_widgets, i);
      g_ptr_array_add (f->gui_parents, gtk_widget_get_parent (widget));
      remember_identity (f, widget);
    }
  g_ptr_array_unref (docks);
}

static void
assert_native_identity (Fixture *f)
{
  GPtrArray *current = g_ptr_array_new ();
  gdouble size;

  g_assert_true (gimp_tools_get_tool_options_gui (f->tool->tool_options) == f->gui);
  g_assert_true (gtk_widget_get_ancestor (f->gui, GIMP_TYPE_TOOL_OPTIONS_EDITOR) == f->editor);
  gather (f->gui, current);
  g_assert_cmpuint (current->len, ==, f->gui_widgets->len);
  for (guint i = 0; i < f->identities->len; i++)
    {
      Identity *identity = g_ptr_array_index (f->identities, i);
      g_assert_cmpuint (identity->life->destroyed, ==, 0);
      g_assert_cmpuint (identity->life->finalized, ==, 0);
      /* Docks can change columns. Their native books, pages and controls must
       * retain their exact owners throughout the topology replay. */
      if (! GIMP_IS_DOCK (identity->widget))
        g_assert_true (gtk_widget_get_parent (identity->widget) == identity->parent);
    }
  for (guint i = 0; i < f->gui_widgets->len; i++)
    {
      GtkWidget *widget = g_ptr_array_index (f->gui_widgets, i);
      g_assert_true (g_ptr_array_find (current, widget, NULL));
      g_assert_true (gtk_widget_get_parent (widget) == g_ptr_array_index (f->gui_parents, i));
    }
  g_object_get (f->tool->tool_options, "brush-size", &size, NULL);
  g_assert_cmpfloat (size, ==, 37.0);
  g_ptr_array_unref (current);
}

static guint
external_dock_count (GimpDialogFactory *factory)
{
  guint result = 0;
  for (GList *iter = gimp_dialog_factory_get_open_dialogs (factory); iter; iter = iter->next)
    if (GIMP_IS_DOCK_WINDOW (iter->data))
      result++;
  return result;
}

static void
assert_final_graph (gboolean painter,
                    gboolean single)
{
  GList *windows = gimp_get_image_windows (gimp);
  GimpImageWindow *window;
  GimpSessionInfo *owner;
  gboolean configured_single, configured_painter;

  g_object_get (gimp->config, "single-window-mode", &configured_single,
                "painter-canvas-ui", &configured_painter, NULL);
  g_assert_cmpint (configured_single, ==, single);
  g_assert_cmpint (configured_painter, ==, painter);
  g_assert_cmpuint (g_list_length (windows), ==, 1);
  window = windows->data;
  g_assert_cmpint (gimp_image_window_is_painter_toolbar (window), ==, painter && ! single);
  g_assert_cmpint (gimp_image_window_get_n_shells (window), ==, 1);
  g_assert_null (gimp_display_get_image (gimp_image_window_get_active_shell (window)->display));
  owner = gimp_dialog_factory_find_session_info (gimp_dialog_factory_get_singleton (),
                                                  single ? "gimp-single-image-window" :
                                                           "gimp-empty-image-window");
  g_assert_nonnull (owner);
  g_assert_true (gimp_session_info_get_widget (owner) == GTK_WIDGET (window));
  g_list_free (windows);
}

static gchar *
read_session (void)
{
  GFile *file = gimp_directory_file ("sessionrc", NULL);
  GError *error = NULL;
  gchar *contents = NULL;
  g_assert_true (g_file_load_contents (file, NULL, &contents, NULL, NULL, &error));
  g_assert_no_error (error);
  g_object_unref (file);
  return contents;
}

static void
assert_file_unchanged (Fixture *f)
{
  gchar *contents = read_session ();
  g_assert_cmpstr (contents, ==, f->original_file);
  g_free (contents);
}

static void
assert_payload_live (Fixture *f)
{
  g_assert_true (gimp_dialog_factory_restore_pending (f->factory));
  g_assert_false (session_is_ready (gimp));
  for (guint i = 0; i < f->targets->len; i++)
    {
      Target *target = g_ptr_array_index (f->targets, i);
      g_assert_cmpuint (target->life->destroyed, ==, 0);
      g_assert_cmpuint (target->life->finalized, ==, 0);
      /* Some peer records can have completed in a nested loop. */
      if (! f->callback_active)
        g_assert_true (target->info->p->docks == target->payload);
    }
}

static void
set_pair (gboolean single,
          gboolean painter)
{
  g_object_set (gimp->config, "single-window-mode", single,
                "painter-canvas-ui", painter, NULL);
}

static void
assert_saved_geometry (Fixture *f)
{
  GPtrArray *docks;
  GimpImageWindow *image_window = NULL;

  if (g_strcmp0 (g_getenv (ENV_PASS), "read") == 0)
    return; /* The fresh reader follows the writer's actual new topology. */
  for (guint i = 0; i < f->targets->len; i++)
    {
      Target *target = g_ptr_array_index (f->targets, i);
      if (GIMP_IS_IMAGE_WINDOW (target->widget))
        image_window = GIMP_IMAGE_WINDOW (target->widget);
    }
  docks = all_docks (f->factory);
  for (guint i = 0; i < docks->len; i++)
    {
      GimpDock *dock = g_ptr_array_index (docks, i);
      gchar *signature = dock_signature (dock);
      if (g_str_equal (signature, dock_signatures[0]))
        {
          GList *books = gimp_dock_get_dockbooks (dock);
          GtkWidget *second = g_list_nth_data (books, 1);
          GtkWidget *parent = gtk_widget_get_parent (second);
          g_assert_true (GTK_IS_PANED (parent));
          g_assert_true (gtk_paned_get_child2 (GTK_PANED (parent)) == second);
          g_assert_cmpint (gtk_paned_get_position (GTK_PANED (parent)), ==, 190);
        }
      if (image_window)
        {
          GimpAlignmentType side = gimp_dock_container_get_dock_side
            (GIMP_DOCK_CONTAINER (image_window), dock);
          if (g_str_equal (signature, dock_signatures[0]))
            g_assert_cmpint (side, ==, GIMP_ALIGN_LEFT);
          else if (g_str_equal (signature, dock_signatures[1]) ||
                   g_str_equal (signature, dock_signatures[2]))
            g_assert_cmpint (side, ==, GIMP_ALIGN_RIGHT);
          else
            /* The accessor returns -1 in its enum type; that enum can have
             * unsigned representation on this compiler. Match its exact
             * sentinel rather than widening it against a signed int literal. */
            g_assert_cmpint (side, ==, (GimpAlignmentType) -1);
          if (g_str_equal (signature, dock_signatures[2]))
            {
              GtkWidget *parent = gtk_widget_get_parent (GTK_WIDGET (dock));
              g_assert_true (GTK_IS_PANED (parent));
              g_assert_true (gtk_paned_get_child2 (GTK_PANED (parent)) == GTK_WIDGET (dock));
              g_assert_cmpint (gtk_paned_get_position (GTK_PANED (parent)), ==, 260);
            }
        }
      g_free (signature);
    }
  if (image_window && GIMP_GUI_CONFIG (gimp->config)->single_window_mode)
    {
      GList *aux = gimp_session_managed_get_aux_info (GIMP_SESSION_MANAGED (image_window));
      gboolean left = FALSE, right = FALSE;
      for (GList *iter = aux; iter; iter = iter->next)
        {
          GimpSessionInfoAux *item = iter->data;
          if (g_str_equal (item->name, "left-docks-width"))
            {
              g_assert_cmpint (g_ascii_strtoll (item->value, NULL, 10), ==, 220);
              left = TRUE;
            }
          if (g_str_equal (item->name, "right-docks-width"))
            {
              g_assert_cmpint (g_ascii_strtoll (item->value, NULL, 10), ==, 260);
              right = TRUE;
            }
        }
      g_assert_true (left && right);
      g_list_free_full (aux, (GDestroyNotify) gimp_session_info_aux_free);
    }
  g_ptr_array_unref (docks);
}

static void
pending_changed (GimpDialogFactory *factory,
                 GParamSpec        *pspec,
                 Fixture           *f)
{
  gboolean pending;
  (void) pspec;
  g_object_get (factory, "restore-pending", &pending, NULL);
  g_assert_cmpint (pending, ==, gimp_dialog_factory_restore_pending (factory));
  if (pending || gimp_dialog_factory_restore_stopping (factory))
    return;
  g_assert_false (f->callback_active);
  f->ready_notifications++;
  if (! f->ready_seen)
    {
      f->ready_seen = TRUE;
      assert_contents (f);
      assert_saved_geometry (f);
      snapshot_native (f);
      for (guint i = 0; i < f->targets->len; i++)
        {
          Target *target = g_ptr_array_index (f->targets, i);
          g_assert_cmpuint (target->life->destroyed, ==, 0);
          g_assert_null (target->info->p->docks);
        }
    }
}

static gboolean
observe_exit (Gimp     *instance,
              gboolean  force,
              gpointer  data)
{
  (void) instance;
  (void) data;
  /* Preserve the parent-owned output path when tests.c performs final exit. */
  g_setenv ("GIMP3_DIRECTORY", g_getenv (ENV_PROFILE), TRUE);
  if (fixture)
    {
      fixture->exit_requests++;
      fixture->last_exit_force = force;
    }
  return FALSE; /* Never intercept or manufacture an accepted exit. */
}

static void
request_accepted_quit (Fixture *f)
{
  guint before = f->exit_requests;
  gimp_exit (gimp, FALSE);
  g_assert_cmpuint (f->exit_requests, ==, before + 1);
  g_assert_false (f->last_exit_force);
  g_assert_false (gimp_test_gui_has_exited (gimp));
  assert_payload_live (f);
  assert_file_unchanged (f);
}

static void
native_page_added (GimpDockbook *book,
                   GimpDockable *page,
                   Fixture      *f)
{
  (void) book;
  f->fills_seen++;
  if (f->interrupt_seen)
    return;
  if (f->test->request != REQUEST_MID_FILL &&
      f->test->request != REQUEST_NESTED_FILL &&
      f->test->request != REQUEST_SAVE_MID_FILL &&
      f->test->request != REQUEST_QUIT_MID_FILL)
    return;

  f->interrupt_seen = TRUE;
  f->first_page = GTK_WIDGET (g_object_ref (page));
  f->first_page_life = watch (G_OBJECT (page));
  f->callback_active = TRUE;
  g_assert_nonnull (gtk_widget_get_parent (GTK_WIDGET (page)));
  g_assert_true (gimp_dialog_factory_restore_pending (f->factory));
  set_pair (f->test->final_single, f->test->final_painter);
  assert_payload_live (f);
  if (f->test->request == REQUEST_SAVE_MID_FILL)
    {
      session_save (gimp, TRUE);
      assert_file_unchanged (f);
    }
  else if (f->test->request == REQUEST_QUIT_MID_FILL)
    request_accepted_quit (f);
  else if (f->test->request == REQUEST_NESTED_FILL)
    {
      guint before = f->fills_seen;
      /* Peers may finish, but the active dispatch still owns native lists. */
      gimp_test_run_mainloop_until_idle ();
      for (guint i = 0; i < 1000 && g_main_context_pending (NULL); i++)
        g_main_context_iteration (NULL, FALSE);
      f->nested_peers_completed = f->fills_seen > before;
      g_assert_true (f->nested_peers_completed);
      g_assert_true (gimp_dialog_factory_restore_pending (f->factory));
      g_assert_false (f->ready_seen);
      g_assert_nonnull (gimp_dockable_get_dockbook (page));
      assert_payload_live (f);
    }
  f->callback_active = FALSE;
}

static void
native_book_added (GimpDock    *dock,
                   GimpDockbook *book,
                   Fixture     *f)
{
  (void) dock;
  g_signal_connect (book, "dockable-added", G_CALLBACK (native_page_added), f);
}

static void
native_dock_added (GimpDockColumns *columns,
                   GimpDock        *dock,
                   Fixture         *f)
{
  (void) columns;
  g_signal_connect (dock, "book-added", G_CALLBACK (native_book_added), f);
}

static void
prepare_gap (Fixture *f)
{
  GPtrArray *roots = g_ptr_array_new ();
  gboolean single;

  f->factory = g_object_ref (gimp_dialog_factory_get_singleton ());
  f->factory_life = watch (G_OBJECT (f->factory));
  f->targets = g_ptr_array_new ();
  f->identities = g_ptr_array_new ();
  f->original_file = read_session ();
  g_assert_true (gimp_dialog_factory_restore_pending (f->factory));
  g_assert_false (gimp_dialog_factory_restore_stopping (f->factory));
  g_assert_false (session_is_ready (gimp));
  g_object_get (gimp->config, "single-window-mode", &single, NULL);
  g_assert_cmpint (single, ==, f->test->single);

  for (GList *iter = gimp_dialog_factory_get_session_infos (f->factory); iter; iter = iter->next)
    {
      GimpSessionInfo *info = iter->data;
      GtkWidget *widget = gimp_session_info_get_widget (info);
      if (info->p->docks && widget)
        {
          Target *target = g_new0 (Target, 1);
          GList *docks;
          target->info = g_object_ref (info);
          target->info_life = watch (G_OBJECT (info));
          target->widget = g_object_ref (widget);
          target->payload = info->p->docks;
          target->life = watch (G_OBJECT (widget));
          g_ptr_array_add (f->targets, target);
          g_assert_true (GIMP_IS_DOCK_CONTAINER (widget));
          docks = gimp_dock_container_get_docks (GIMP_DOCK_CONTAINER (widget));
          g_assert_null (docks); /* Failure to observe the gap fails the fixture. */
          gather (widget, roots);
        }
    }
  if (g_strcmp0 (g_getenv (ENV_PASS), "read") == 0)
    g_assert_cmpuint (f->targets->len, >, 0);
  else
    g_assert_cmpuint (f->targets->len, ==, f->test->single ? 2 : 4);
  for (guint i = 0; i < roots->len; i++)
    if (GIMP_IS_DOCK_COLUMNS (g_ptr_array_index (roots, i)))
      g_signal_connect (g_ptr_array_index (roots, i), "dock-added",
                        G_CALLBACK (native_dock_added), f);
  g_ptr_array_unref (roots);
  f->pending_handler = g_signal_connect (f->factory, "notify::restore-pending",
                                          G_CALLBACK (pending_changed), f);
  f->tool = GIMP_TOOL_INFO (gimp_container_get_child_by_name (gimp->tool_info_list,
                                                             "gimp-paintbrush-tool"));
  g_assert_nonnull (f->tool);
  gimp_context_set_tool (gimp_get_user_context (gimp), f->tool);
  g_object_set (f->tool->tool_options, "brush-size", 37.0, NULL);
  assert_payload_live (f);
}

static void
assert_saved_file (const gchar *contents,
                   gboolean     single)
{
  for (guint i = 0; i < G_N_ELEMENTS (identifiers); i++)
    {
      gchar *needle = g_strdup_printf ("(dockable \"%s\"", identifiers[i]);
      const gchar *first = strstr (contents, needle);
      g_assert_nonnull (first);
      g_assert_null (strstr (first + strlen (needle), needle));
      g_free (needle);
    }
  g_assert_nonnull (strstr (contents, single ? "(single-window-mode yes)" :
                                             "(single-window-mode no)"));
  g_assert_null (strstr (contents, "painter-toolbar-window-controls"));
}

static void
release_fixture_refs (Fixture *f)
{
  if (f->pending_handler)
    {
      /* Accepted Quit disposes the singleton even while this fixture keeps
       * its final lifetime reference. Disposal may have removed our handler. */
      if (g_signal_handler_is_connected (f->factory, f->pending_handler))
        g_signal_handler_disconnect (f->factory, f->pending_handler);
      else
        {
          g_assert_true (gimp_test_gui_has_exited (gimp));
          g_assert_true (gimp_dialog_factory_restore_stopping (f->factory));
        }
      f->pending_handler = 0;
    }
  for (guint i = 0; i < f->identities->len; i++)
    {
      Identity *identity = g_ptr_array_index (f->identities, i);
      g_object_unref (identity->widget);
      g_free (identity);
    }
  g_ptr_array_unref (f->identities);
  for (guint i = 0; i < f->targets->len; i++)
    {
      Target *target = g_ptr_array_index (f->targets, i);
      g_object_unref (target->widget);
      g_object_unref (target->info);
      if (gimp_test_gui_has_exited (gimp))
        {
          g_assert_cmpuint (target->life->finalized, ==, 1);
          g_assert_cmpuint (target->info_life->finalized, ==, 1);
        }
      g_free (target);
    }
  g_ptr_array_unref (f->targets);
  g_clear_pointer (&f->gui_widgets, g_ptr_array_unref);
  g_clear_pointer (&f->gui_parents, g_ptr_array_unref);
  g_clear_object (&f->first_page);
  g_clear_object (&f->recent_info);
  g_clear_object (&f->recent_page);
  g_clear_object (&f->factory);
  if (gimp_test_gui_has_exited (gimp))
    g_assert_cmpuint (f->factory_life->finalized, ==, 1);
  g_free (f->original_file);
  /* f remains live for still-connected native callbacks until process exit. */
}

/* Separate factories make terminal cancellation observable without putting the
 * application's singleton into an artificial half-shutdown state. The entries,
 * widgets and fill algorithm below are the real native ones. */
typedef struct
{
  GimpDialogFactory    *factory;
  GimpDialogRestoreFunc native_restore;
  Request              request;
  GimpSessionInfo     *infos[3];
  Lifetime           *info_lives[3];
  Lifetime           *window_lives[3];
  guint               constructed;
  guint               pages;
  guint               ready;
  gboolean            nested;
  gboolean            active;
  gboolean            disposed_inside;
} OwnedFactory;

static GimpSessionInfo *
new_saved_dock (GimpDialogFactory *factory,
                const gchar       *identifier)
{
  GimpSessionInfo *info = gimp_session_info_new ();
  GimpSessionInfoDock *dock = gimp_session_info_dock_new ("gimp-dock");
  GimpSessionInfoBook *book = gimp_session_info_book_new ();
  GimpSessionInfoDockable *page = gimp_session_info_dockable_new ();

  page->identifier = g_strdup (identifier);
  page->tab_style = GIMP_TAB_STYLE_ICON;
  page->view_size = -1;
  book->dockables = g_list_append (NULL, page);
  dock->books = g_list_append (NULL, book);
  info->p->docks = g_list_append (NULL, dock);
  info->p->open = TRUE;
  info->p->width = 260;
  info->p->height = 400;
  gimp_session_info_set_factory_entry (info,
    gimp_dialog_factory_find_entry (factory, "gimp-dock-window"));
  gimp_dialog_factory_add_session_info (factory, info);
  return info;
}

static void
owned_pending_changed (GimpDialogFactory *factory,
                       GParamSpec        *pspec,
                       OwnedFactory      *state)
{
  (void) pspec;
  if (! gimp_dialog_factory_restore_pending (factory) &&
      ! gimp_dialog_factory_restore_stopping (factory))
    {
      g_assert_false (state->active);
      state->ready++;
    }
}

static void
owned_page_added (GimpDockbook *book,
                   GimpDockable *page,
                   OwnedFactory *state)
{
  (void) book;
  (void) page;
  state->pages++;
  if (state->request != REQUEST_DISPOSE_ACTIVE || state->disposed_inside)
    return;
  state->active = TRUE;
  state->disposed_inside = TRUE;
  g_object_run_dispose (G_OBJECT (state->factory));
  g_assert_true (gimp_dialog_factory_restore_stopping (state->factory));
  g_assert_true (gimp_dialog_factory_restore_pending (state->factory));
  for (guint i = 0; i < state->constructed; i++)
    g_assert_cmpuint (state->window_lives[i]->destroyed, ==, 0);
  /* Cancellation removes queued peers without freeing the active lists or
   * destroying its native page/book while this callback is still on stack. */
  gimp_test_run_mainloop_until_idle ();
  g_assert_cmpuint (state->pages, ==, 1);
  for (guint i = 0; i < state->constructed; i++)
    g_assert_cmpuint (state->window_lives[i]->destroyed, ==, 0);
  g_assert_nonnull (gimp_dockable_get_dockbook (page));
  state->active = FALSE;
}

static void
owned_book_added (GimpDock *dock,
                  GimpDockbook *book,
                  OwnedFactory *state)
{
  (void) dock;
  g_signal_connect (book, "dockable-added", G_CALLBACK (owned_page_added), state);
}

static void
owned_dock_added (GimpDockColumns *columns,
                  GimpDock        *dock,
                  OwnedFactory    *state)
{
  (void) columns;
  g_signal_connect (dock, "book-added", G_CALLBACK (owned_book_added), state);
}

static GtkWidget *
owned_restore_window (GimpDialogFactory *factory,
                      GdkMonitor        *monitor,
                      GimpSessionInfo   *info)
{
  OwnedFactory *state = g_object_get_data (G_OBJECT (factory), "test-owned-restore");
  GtkWidget *window = state->native_restore (factory, monitor, info);
  GPtrArray *widgets = g_ptr_array_new ();
  guint index = state->constructed++;

  g_assert_cmpuint (index, <, G_N_ELEMENTS (state->window_lives));
  state->window_lives[index] = watch (G_OBJECT (window));
  gather (window, widgets);
  for (guint i = 0; i < widgets->len; i++)
    if (GIMP_IS_DOCK_COLUMNS (g_ptr_array_index (widgets, i)))
      g_signal_connect (g_ptr_array_index (widgets, i), "dock-added",
                        G_CALLBACK (owned_dock_added), state);
  g_ptr_array_unref (widgets);
  if (state->request == REQUEST_DISPOSE_CONSTRUCTOR)
    {
      state->active = TRUE;
      state->disposed_inside = TRUE;
      g_assert_cmpuint (index, ==, 0);
      g_object_run_dispose (G_OBJECT (factory));
      g_assert_true (gimp_dialog_factory_restore_stopping (factory));
      g_assert_true (gimp_dialog_factory_restore_pending (factory));
      g_assert_true (gimp_session_info_get_widget (info) == window);
      g_assert_nonnull (info->p->docks);
      g_assert_cmpuint (state->window_lives[index]->destroyed, ==, 0);
      g_assert_cmpuint (state->window_lives[index]->finalized, ==, 0);
      g_assert_cmpuint (state->info_lives[index]->finalized, ==, 0);
      gimp_test_run_mainloop_until_idle ();
      g_assert_cmpuint (state->constructed, ==, 1);
      g_assert_cmpuint (state->pages, ==, 0);
      g_assert_cmpuint (state->ready, ==, 0);
      g_assert_cmpuint (state->window_lives[index]->destroyed, ==, 0);
      g_assert_nonnull (info->p->docks);
      state->active = FALSE;
    }
  if (state->request == REQUEST_ENUMERATION && index == 1 && ! state->nested)
    {
      state->nested = TRUE;
      state->active = TRUE;
      /* The inner walk schedules the third original entry before the outer
       * walk reaches it. Finishing those peers must not release readiness or
       * enroll the completed entry twice when the outer walk resumes. */
      gimp_dialog_factory_restore (factory, monitor);
      gimp_test_run_mainloop_until_idle ();
      g_assert_cmpuint (state->pages, ==, 2);
      g_assert_null (state->infos[2]->p->docks);
      state->infos[2]->p->open = TRUE;
      g_assert_true (gimp_dialog_factory_restore_pending (factory));
      g_assert_cmpuint (state->ready, ==, 0);
      state->active = FALSE;
    }
  return window;
}

static void
exercise_owned_factory (Request request)
{
  const gchar *entries[] = { "gimp-dock-window", "gimp-dock", "gimp-error-console",
                             "gimp-buffer-list", "gimp-image-list" };
  const gchar *pages[] = { "gimp-error-console", "gimp-buffer-list", "gimp-image-list" };
  OwnedFactory state = { 0 };
  GimpDialogFactory *native = gimp_dialog_factory_get_singleton ();
  GdkMonitor *monitor = gdk_display_get_primary_monitor (gdk_display_get_default ());
  Lifetime *factory_life;
  guint expected_windows = request == REQUEST_DISPOSE_CONSTRUCTOR ? 1 : 3;
  gint64 deadline;

  state.request = request;
  state.factory = gimp_dialog_factory_new ("restore-barrier-owned",
                                            gimp_get_user_context (gimp));
  factory_life = watch (G_OBJECT (state.factory));
  g_object_set_data (G_OBJECT (state.factory), "test-owned-restore", &state);
  for (guint i = 0; i < G_N_ELEMENTS (entries); i++)
    {
      GimpDialogFactoryEntry *entry = gimp_dialog_factory_find_entry (native, entries[i]);
      g_assert_nonnull (entry);
      if (i == 0)
        state.native_restore = entry->restore_func;
      gimp_dialog_factory_register_entry
        (state.factory, entry->identifier, entry->name, entry->blurb, entry->icon_name,
         entry->help_id, entry->new_func, i == 0 ? owned_restore_window : entry->restore_func,
         entry->view_size, entry->singleton, entry->session_managed, entry->remember_size,
         entry->remember_if_open, entry->hideable, entry->image_window, entry->dockable);
    }
  for (guint i = 0; i < G_N_ELEMENTS (pages); i++)
    {
      state.infos[i] = new_saved_dock (state.factory, pages[i]);
      state.info_lives[i] = watch (G_OBJECT (state.infos[i]));
    }
  g_signal_connect (state.factory, "notify::restore-pending",
                    G_CALLBACK (owned_pending_changed), &state);
  gimp_dialog_factory_restore (state.factory, monitor);
  g_assert_cmpint (gimp_dialog_factory_restore_pending (state.factory), ==,
                   request != REQUEST_DISPOSE_CONSTRUCTOR);
  g_assert_cmpuint (state.constructed, ==, expected_windows);
  if (request == REQUEST_DISPOSE_QUEUED)
    {
      g_assert_cmpuint (state.pages, ==, 0);
      g_object_run_dispose (G_OBJECT (state.factory));
    }
  deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (gimp_dialog_factory_restore_pending (state.factory) &&
         g_get_monotonic_time () < deadline)
    g_main_context_iteration (NULL, FALSE);
  g_assert_false (gimp_dialog_factory_restore_pending (state.factory));
  if (request == REQUEST_ENUMERATION)
    {
      g_assert_true (state.nested);
      g_assert_cmpuint (state.pages, ==, 3);
      g_assert_cmpuint (state.ready, ==, 1);
      g_assert_false (gimp_dialog_factory_restore_stopping (state.factory));
      g_object_run_dispose (G_OBJECT (state.factory));
    }
  else
    {
      g_assert_cmpuint (state.pages, ==, request == REQUEST_DISPOSE_ACTIVE ? 1 : 0);
      g_assert_cmpuint (state.ready, ==, 0); /* Cancellation is never readiness. */
    }
  g_assert_true (gimp_dialog_factory_restore_stopping (state.factory));
  for (guint i = 0; i < expected_windows; i++)
    {
      g_assert_cmpuint (state.window_lives[i]->destroyed, ==, 1);
      g_assert_cmpuint (state.window_lives[i]->finalized, ==, 1);
    }
  for (guint i = 0; i < G_N_ELEMENTS (pages); i++)
    {
      g_object_unref (state.infos[i]);
      g_assert_cmpuint (state.info_lives[i]->finalized, ==, 1);
    }
  /* Terminal restore calls and the post-dispose event drain must stay inert. */
  gimp_dialog_factory_restore (state.factory, monitor);
  gimp_test_run_mainloop_until_idle ();
  g_assert_cmpuint (state.constructed, ==, expected_windows);
  g_object_unref (state.factory);
  g_assert_cmpuint (factory_life->finalized, ==, 1);
}

static void
restore_during_conversion (GtkWidget *widget,
                           GtkWidget *previous,
                           Fixture   *f)
{
  if (! previous || gtk_widget_get_parent (widget))
    return;
  if (f->conversion_interrupted)
    {
      if (gimp_dialog_factory_restore_pending (f->factory))
        f->pending_transfer_removals++;
      return;
    }
  f->conversion_interrupted = TRUE;
  g_assert_false (gimp_dialog_factory_restore_stopping (f->factory));
  f->extra_dock = TRUE;
  f->recent_info = new_saved_dock (f->factory, "gimp-error-console");
  /* This is the same direct native restore entry point used by recent docks,
   * outside the startup factory batch. */
  gimp_session_info_restore (f->recent_info, f->factory,
                             gdk_display_get_primary_monitor (gdk_display_get_default ()));
  g_assert_true (gimp_dialog_factory_restore_pending (f->factory));
  f->recent_target_life = watch (G_OBJECT (gimp_session_info_get_widget (f->recent_info)));
  set_pair (FALSE, TRUE);
  session_save (gimp, TRUE);
  assert_file_unchanged (f);
  if (f->test->request == REQUEST_RESTORE_DURING_CONVERSION_PENDING)
    return; /* The outer conversion must unwind and wait for this fill. */
  gimp_test_run_mainloop_until_idle ();
  for (guint i = 0; i < 1000 && g_main_context_pending (NULL); i++)
    g_main_context_iteration (NULL, FALSE);
  g_assert_false (gimp_dialog_factory_restore_pending (f->factory));
  /* Native readiness cannot save a still-active, interrupted topology pass. */
  g_assert_false (session_is_ready (gimp));
  assert_file_unchanged (f);
  f->recent_page = g_object_ref (gimp_dialog_factory_find_widget (f->factory,
                                                                 "gimp-error-console"));
  g_assert_true (GIMP_IS_DOCKABLE (f->recent_page));
  f->recent_life = watch (G_OBJECT (f->recent_page));
}

static void
exercise_restore_during_conversion (Fixture *f)
{
  GPtrArray *docks = all_docks (f->factory);
  gchar *contents;

  for (guint i = 0; i < docks->len; i++)
    g_signal_connect (g_ptr_array_index (docks, i), "parent-set",
                      G_CALLBACK (restore_during_conversion), f);
  set_pair (TRUE, TRUE);
  g_assert_true (f->conversion_interrupted);
  if (f->test->request == REQUEST_RESTORE_DURING_CONVERSION_PENDING)
    {
      g_assert_true (gimp_dialog_factory_restore_pending (f->factory));
      g_assert_false (session_is_ready (gimp));
      g_assert_nonnull (f->recent_info->p->docks);
      g_assert_cmpuint (f->pending_transfer_removals, ==, 0);
      g_assert_cmpuint (f->recent_target_life->destroyed, ==, 0);
      assert_file_unchanged (f);
    }
  settle ();
  if (! f->recent_page)
    {
      f->recent_page = g_object_ref (gimp_dialog_factory_find_widget (f->factory,
                                                                     "gimp-error-console"));
      g_assert_true (GIMP_IS_DOCKABLE (f->recent_page));
      f->recent_life = watch (G_OBJECT (f->recent_page));
    }
  g_assert_true (f->conversion_interrupted);
  g_assert_true (session_is_ready (gimp));
  g_assert_false (gimp_dialog_factory_restore_stopping (f->factory));
  g_assert_cmpuint (f->ready_notifications, ==, 2);
  assert_contents (f);
  assert_native_identity (f);
  assert_final_graph (TRUE, FALSE);
  g_assert_true (gimp_dialog_factory_find_widget (f->factory, "gimp-error-console") ==
                 f->recent_page);
  g_assert_cmpuint (f->recent_life->destroyed, ==, 0);
  contents = read_session ();
  assert_saved_file (contents, FALSE);
  g_assert_nonnull (strstr (contents, "(dockable \"gimp-error-console\""));
  g_free (contents);
  for (guint i = 0; i < docks->len; i++)
    g_signal_handlers_disconnect_by_func (g_ptr_array_index (docks, i),
                                          restore_during_conversion, f);
  g_ptr_array_unref (docks);
}

static void
run_startup (const RestoreCase *test)
{
  Fixture *f = g_new0 (Fixture, 1);
  gboolean reader = g_strcmp0 (g_getenv (ENV_PASS), "read") == 0;
  RestoreCase read_case;
  GimpDisplay *dirty_display = NULL;
  GimpImage *dirty_image = NULL;
  GimpDisplay *displays[2] = { NULL, NULL };
  GimpImage *images[2] = { NULL, NULL };
  guint image_count = ! reader ? (test->request == REQUEST_ONE_IMAGE ? 1 :
                                  test->request == REQUEST_TWO_IMAGES ? 2 : 0) : 0;

  fixture = f;
  if (reader)
    {
      read_case = *test;
      read_case.single = test->final_single;
      read_case.painter = test->final_painter;
      read_case.request = REQUEST_NONE;
      f->test = &read_case;
      f->extra_dock = test->request == REQUEST_RESTORE_DURING_CONVERSION ||
                      test->request == REQUEST_RESTORE_DURING_CONVERSION_PENDING;
    }
  else
    f->test = test;

  if (test->request == REQUEST_NO_RESTORE)
    {
      GimpDialogFactory *factory = gimp_dialog_factory_get_singleton ();
      g_assert_false (gimp_dialog_factory_restore_pending (factory));
      set_pair (test->final_single, test->final_painter);
      settle ();
      assert_final_graph (test->final_painter, test->final_single);
      g_assert_true (session_is_ready (gimp));
      fixture = NULL;
      return;
    }

  prepare_gap (f);
  for (guint i = 0; i < image_count; i++)
    {
      gimp_test_utils_create_image (gimp, 64 + i, 64 + i);
      displays[i] = gimp_context_get_display (gimp_get_user_context (gimp));
      images[i] = gimp_display_get_image (displays[i]);
      g_assert_nonnull (images[i]);
    }
  if (image_count)
    gimp_context_set_display (gimp_get_user_context (gimp), displays[0]);
  if (! reader)
    switch (test->request)
      {
      case REQUEST_CHANGE:
      case REQUEST_ONE_IMAGE:
      case REQUEST_TWO_IMAGES:
      case REQUEST_SAVE_EARLY:
      case REQUEST_QUIT_EARLY:
      case REQUEST_SAVE_CLEAR:
      case REQUEST_SAVE_ALWAYS:
      case REQUEST_SAVE_THEN_CLEAR:
        set_pair (test->final_single, test->final_painter);
        break;
      case REQUEST_LATEST:
      case REQUEST_NET_ZERO:
        set_pair (! test->single, ! test->painter);
        set_pair (test->single, ! test->painter);
        set_pair (! test->single, test->painter);
        set_pair (test->final_single, test->final_painter);
        break;
      case REQUEST_PAINTER_ONLY:
        g_object_set (gimp->config, "painter-canvas-ui", test->final_painter, NULL);
        break;
      case REQUEST_UNCHANGED:
        g_object_notify (G_OBJECT (gimp->config), "single-window-mode");
        g_object_notify (G_OBJECT (gimp->config), "painter-canvas-ui");
        break;
      case REQUEST_REPEAT_BATCH:
        /* Configure events can mark an in-flight session record open again.
         * A second factory walk must not enqueue its saved docks twice. */
        for (guint i = 0; i < f->targets->len; i++)
          ((Target *) g_ptr_array_index (f->targets, i))->info->p->open = TRUE;
        gimp_dialog_factory_restore (f->factory, gdk_display_get_primary_monitor
                                                 (gdk_display_get_default ()));
        break;
      default:
        break;
      }
  assert_payload_live (f);
  if (! reader && test->request == REQUEST_SAVE_EARLY)
    {
      session_save (gimp, FALSE);
      session_save (gimp, TRUE);
      assert_file_unchanged (f);
    }
  if (! reader && (test->request == REQUEST_SAVE_CLEAR ||
                  test->request == REQUEST_SAVE_ALWAYS ||
                  test->request == REQUEST_SAVE_THEN_CLEAR))
    {
      GError *error = NULL;
      GFile *file = gimp_directory_file ("sessionrc", NULL);
      if (test->request == REQUEST_SAVE_THEN_CLEAR)
        session_save (gimp, TRUE);
      g_assert_true (session_clear (gimp, &error));
      g_assert_no_error (error);
      session_save (gimp, FALSE);
      if (test->request == REQUEST_SAVE_ALWAYS)
        session_save (gimp, TRUE);
      g_assert_false (g_file_query_exists (file, NULL));
      g_object_unref (file);
    }
  if (! reader && test->request == REQUEST_QUIT_EARLY)
    request_accepted_quit (f);
  if (! reader && test->request == REQUEST_DIRTY_CANCEL)
    {
      GtkWidget *dialog;
      gimp_test_utils_create_image (gimp, 64, 64);
      dirty_display = gimp_context_get_display (gimp_get_user_context (gimp));
      dirty_image = gimp_display_get_image (dirty_display);
      g_assert_nonnull (dirty_image);
      gimp_image_dirty (dirty_image, GIMP_DIRTY_IMAGE);
      gimp_exit (gimp, FALSE);
      g_assert_cmpuint (f->exit_requests, ==, 1);
      g_assert_false (gimp_test_gui_has_exited (gimp));
      dialog = gimp_dialog_factory_find_widget (f->factory, "gimp-quit-dialog");
      g_assert_true (GTK_IS_DIALOG (dialog));
      g_assert_true (gtk_widget_get_visible (dialog));
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
      set_pair (test->final_single, test->final_painter);
      assert_payload_live (f);
      assert_file_unchanged (f);
    }

  settle ();
  g_assert_true (f->ready_seen);
  g_assert_cmpuint (f->ready_notifications, ==, 1);
  if (! reader && (test->request == REQUEST_MID_FILL ||
                  test->request == REQUEST_NESTED_FILL ||
                  test->request == REQUEST_SAVE_MID_FILL ||
                  test->request == REQUEST_QUIT_MID_FILL))
    g_assert_true (f->interrupt_seen);

  if (! reader && (test->request == REQUEST_QUIT_EARLY ||
                  test->request == REQUEST_QUIT_MID_FILL))
    {
      gchar *contents;
      g_assert_true (gimp_test_gui_has_exited (gimp));
      g_assert_cmpuint (f->exit_requests, ==, 2);
      g_assert_false (f->last_exit_force); /* The retry must not escalate force. */
      g_assert_null (gimp_get_display_iter (gimp));
      g_assert_false (gimp_dialog_factory_restore_pending (f->factory));
      g_assert_true (gimp_dialog_factory_restore_stopping (f->factory));
      for (guint i = 0; i < f->targets->len; i++)
        g_assert_cmpuint (((Target *) g_ptr_array_index (f->targets, i))->life->destroyed, ==, 1);
      contents = read_session ();
      assert_saved_file (contents, test->final_single);
      g_free (contents);
      /* Exercise app.c's post-exit event drain with real torn-down objects. */
      gimp_test_run_mainloop_until_idle ();
      g_assert_cmpuint (f->ready_notifications, ==, 1);
      g_assert_cmpuint (f->exit_requests, ==, 2);
    }
  else
    {
      g_assert_false (gimp_test_gui_has_exited (gimp));
      g_assert_true (session_is_ready (gimp));
      assert_contents (f);
      assert_native_identity (f);
      if (f->first_page)
        {
          g_assert_cmpuint (f->first_page_life->destroyed, ==, 0);
          g_assert_true (gimp_dialog_factory_find_widget
                         (f->factory, dockable_identifier (f->first_page)) == f->first_page);
        }
      if (dirty_display)
        {
          g_assert_cmpuint (f->exit_requests, ==, 1);
          assert_file_unchanged (f);
          g_assert_cmpint (gimp_image_is_dirty (dirty_image), !=, 0);
          gimp_image_clean_all (dirty_image);
          gimp_display_close (dirty_display);
          g_object_unref (dirty_image);
          settle ();
        }
      if (image_count)
        {
          GList *windows = gimp_get_image_windows (gimp);
          guint hosts = 0;
          g_assert_true (gimp_context_get_display (gimp_get_user_context (gimp)) == displays[0]);
          g_assert_cmpuint (g_list_length (windows), ==,
                           test->final_single ? 1 : image_count + (test->final_painter ? 1 : 0));
          for (GList *iter = windows; iter; iter = iter->next)
            hosts += gimp_image_window_is_painter_toolbar (iter->data);
          g_assert_cmpuint (hosts, ==, test->final_painter && ! test->final_single ? 1 : 0);
          g_list_free (windows);
          for (guint i = 0; i < image_count; i++)
            {
              GimpDisplayShell *shell = gimp_display_get_shell (displays[i]);
              GimpImageWindow *window = gimp_display_shell_get_window (shell);
              gboolean member = FALSE;
              g_assert_true (gimp_display_get_image (displays[i]) == images[i]);
              g_assert_true (gimp_container_have (gimp->displays, GIMP_OBJECT (displays[i])));
              for (gint j = 0; j < gimp_image_window_get_n_shells (window); j++)
                member |= gimp_image_window_get_shell (window, j) == shell;
              g_assert_true (member);
            }
          for (guint i = 0; i < image_count; i++)
            {
              gimp_image_clean_all (images[i]);
              gimp_display_close (displays[i]);
              g_object_unref (images[i]);
            }
          settle ();
        }
      assert_final_graph (test->final_painter, test->final_single);
      if (! reader && (test->request == REQUEST_NET_ZERO ||
                      test->request == REQUEST_PAINTER_ONLY ||
                      test->request == REQUEST_NONE ||
                      test->request == REQUEST_UNCHANGED))
        {
          g_assert_cmpuint (external_dock_count (f->factory), ==, test->single ? 1 : 4);
          for (guint i = 0; i < f->targets->len; i++)
            g_assert_cmpuint (((Target *) g_ptr_array_index (f->targets, i))->life->destroyed, ==, 0);
        }
      if (! reader && (test->request == REQUEST_SAVE_CLEAR ||
                      test->request == REQUEST_SAVE_THEN_CLEAR))
        {
          GFile *file = gimp_directory_file ("sessionrc", NULL);
          g_assert_false (g_file_query_exists (file, NULL));
          g_object_unref (file);
          /* Keep ordinary exit from changing the tested cleared-file result. */
          g_object_set (gimp->config, "save-session-info", FALSE, NULL);
        }
      if (! reader && (test->request == REQUEST_SAVE_EARLY ||
                      test->request == REQUEST_SAVE_MID_FILL ||
                      test->request == REQUEST_SAVE_ALWAYS))
        {
          gchar *contents = read_session ();
          assert_saved_file (contents, test->final_single);
          g_free (contents);
        }
    }
  if (! reader && (test->request == REQUEST_RESTORE_DURING_CONVERSION ||
                  test->request == REQUEST_RESTORE_DURING_CONVERSION_PENDING))
    exercise_restore_during_conversion (f);
  if (! reader && (test->request == REQUEST_ENUMERATION ||
                  test->request == REQUEST_DISPOSE_QUEUED ||
                  test->request == REQUEST_DISPOSE_ACTIVE ||
                  test->request == REQUEST_DISPOSE_CONSTRUCTOR))
    exercise_owned_factory (test->request);
  if (dirty_display)
    {
      /* A cancelled dirty Quit must not leave a queued request. The later
       * explicit clean Quit still takes the ordinary, accepted lifecycle. */
      gimp_exit (gimp, FALSE);
      g_assert_true (gimp_test_gui_has_exited (gimp));
      g_assert_cmpuint (f->exit_requests, ==, 2);
      g_assert_false (f->last_exit_force);
      gimp_test_run_mainloop_until_idle ();
    }
  release_fixture_refs (f);
  f->test = test;
  fixture = NULL;
}

static void
write_fixture (const gchar *directory,
               gboolean     single)
{
  GString *session = g_string_new ("# Isolated native restore-barrier fixture\n");
  gchar *path = g_build_filename (directory, "sessionrc", NULL);
  GError *error = NULL;

  if (single)
    {
      g_string_append (session,
                       "(session-info \"toplevel\" (factory-entry \"gimp-single-image-window\")"
                       " (position 20 30) (size 950 700) (open-on-exit)"
                       " (aux-info (left-docks-width \"220\") (right-docks-width \"260\"))");
      for (guint i = 0; i < 3; i++)
        g_string_append_printf (session, " (gimp-dock (side %s) (position %u) %s)",
                                 i ? "right" : "left", i == 2 ? 260 : 0, dock_bodies[i]);
      g_string_append (session, ")\n");
    }
  for (guint i = single ? 3 : 0; i < G_N_ELEMENTS (dock_bodies); i++)
    g_string_append_printf (session,
                             "(session-info \"toplevel\" (factory-entry \"gimp-dock-window\")"
                             " (position %u %u) (size 260 580) (open-on-exit)"
                             " (aux-info (show-image-menu \"false\") (follow-active-image \"true\"))"
                             " (gimp-dock %s))\n",
                             80 + i * 270, 80 + i * 30, dock_bodies[i]);
  g_string_append (session,
                   "(session-info \"toplevel\" (factory-entry \"gimp-empty-image-window\")"
                   " (position 120 30) (size 680 250))\n");
  if (! single)
    g_string_append (session,
                     "(session-info \"toplevel\" (factory-entry \"gimp-single-image-window\")"
                     " (position 20 30) (size 950 700))\n");
  g_string_append_printf (session, "(single-window-mode %s)\n", single ? "yes" : "no");
  g_assert_true (g_file_set_contents (path, session->str, session->len, &error));
  g_assert_no_error (error);
  g_free (path);
  g_string_free (session, TRUE);
}

static void
remove_profile (GFile *file)
{
  GError *error = NULL;
  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) ==
      G_FILE_TYPE_DIRECTORY)
    {
      GFileEnumerator *children = g_file_enumerate_children
        (file, G_FILE_ATTRIBUTE_STANDARD_NAME, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
      GFileInfo *info;
      g_assert_no_error (error);
      g_assert_nonnull (children);
      while ((info = g_file_enumerator_next_file (children, NULL, &error)))
        {
          GFile *child = g_file_get_child (file, g_file_info_get_name (info));
          remove_profile (child);
          g_object_unref (child);
          g_object_unref (info);
        }
      g_assert_no_error (error);
      g_assert_true (g_file_enumerator_close (children, NULL, &error));
      g_assert_no_error (error);
      g_object_unref (children);
    }
  g_assert_true (g_file_delete (file, NULL, &error));
  g_assert_no_error (error);
}

static void
assert_child_clean (void)
{
  const gchar *patterns[] = { "*WARNING*", "*CRITICAL*", "*ERROR*", "*Warning:*",
                              "*Critical:*", "*Error:*", "*warning:*", "*critical:*", "*error:*" };
  for (guint i = 0; i < G_N_ELEMENTS (patterns); i++)
    g_test_trap_assert_stdout_unmatched (patterns[i]);
}

static void
run_case (gconstpointer data)
{
  const RestoreCase *test = data;
  GError *error = NULL;
  gchar *directory;
  GFile *profile;

  if (g_test_subprocess ())
    {
      run_startup (test);
      return;
    }
  directory = g_dir_make_tmp ("gimp-restore-barrier-XXXXXX", &error);
  g_assert_no_error (error);
  g_assert_nonnull (directory);
  profile = g_file_new_for_path (directory);
  write_fixture (directory, test->single);
  g_setenv (ENV_PROFILE, directory, TRUE);
  g_setenv (ENV_RESTORE, test->request == REQUEST_NO_RESTORE ? "0" : "1", TRUE);
  for (guint pass = 0; pass < (test->fresh_reader ? 2 : 1); pass++)
    {
      g_setenv (ENV_PASS, pass ? "read" : "write", TRUE);
      /* The reader's initially opposite mode must be replaced by the exact
       * writer output during session_init(), before its native restore. */
      g_setenv (ENV_SINGLE, (pass ? ! test->final_single : test->single) ? "1" : "0", TRUE);
      g_setenv (ENV_PAINTER, (pass ? test->final_painter : test->painter) ? "1" : "0", TRUE);
      g_test_trap_subprocess (NULL, 60 * G_USEC_PER_SEC, G_TEST_SUBPROCESS_INHERIT_STDERR);
      g_test_trap_assert_passed ();
      assert_child_clean ();
      if (test->fresh_reader)
        {
          gchar *path = g_build_filename (directory, "sessionrc", NULL);
          gchar *contents = NULL;
          g_assert_true (g_file_get_contents (path, &contents, NULL, &error));
          g_assert_no_error (error);
          assert_saved_file (contents, test->final_single);
          g_free (contents);
          g_free (path);
        }
    }
  g_unsetenv (ENV_PROFILE);
  g_unsetenv (ENV_RESTORE);
  g_unsetenv (ENV_SINGLE);
  g_unsetenv (ENV_PAINTER);
  g_unsetenv (ENV_PASS);
  remove_profile (profile);
  g_object_unref (profile);
  g_free (directory);
}

int
main (int argc, char **argv)
{
  static const RestoreCase cases[] = {
    { "changed/multi-single-native", REQUEST_CHANGE, FALSE, FALSE, TRUE, FALSE, FALSE },
    { "changed/multi-single-painter", REQUEST_CHANGE, FALSE, TRUE, TRUE, TRUE, FALSE },
    { "changed/single-multi-native", REQUEST_CHANGE, TRUE, FALSE, FALSE, FALSE, FALSE },
    { "changed/single-multi-painter", REQUEST_CHANGE, TRUE, TRUE, FALSE, TRUE, FALSE },
    { "selection/one-image-multi-single", REQUEST_ONE_IMAGE, FALSE, TRUE, TRUE, TRUE, FALSE },
    { "selection/two-images-single-multi", REQUEST_TWO_IMAGES, TRUE, TRUE, FALSE, TRUE, FALSE },
    { "latest/from-multi", REQUEST_LATEST, FALSE, FALSE, TRUE, TRUE, FALSE },
    { "latest/from-single", REQUEST_LATEST, TRUE, TRUE, FALSE, FALSE, FALSE },
    { "net-zero/multi", REQUEST_NET_ZERO, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "net-zero/single", REQUEST_NET_ZERO, TRUE, FALSE, TRUE, FALSE, FALSE },
    { "painter-only/single-enable", REQUEST_PAINTER_ONLY, TRUE, FALSE, TRUE, TRUE, FALSE },
    { "painter-only/single-disable", REQUEST_PAINTER_ONLY, TRUE, TRUE, TRUE, FALSE, FALSE },
    { "controls/no-request-multi", REQUEST_NONE, FALSE, FALSE, FALSE, FALSE, FALSE },
    { "controls/no-request-single", REQUEST_NONE, TRUE, TRUE, TRUE, TRUE, FALSE },
    { "controls/unchanged-notifications", REQUEST_UNCHANGED, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "controls/no-restore", REQUEST_NO_RESTORE, FALSE, FALSE, TRUE, TRUE, FALSE },
    { "controls/repeated-pending-batch", REQUEST_REPEAT_BATCH, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "ownership/queued-cancellation", REQUEST_DISPOSE_QUEUED, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "ownership/constructor-disposal", REQUEST_DISPOSE_CONSTRUCTOR, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "ownership/active-disposal", REQUEST_DISPOSE_ACTIVE, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "interruption/pending-recent-dock-during-conversion", REQUEST_RESTORE_DURING_CONVERSION_PENDING, FALSE, TRUE, FALSE, TRUE, TRUE },
    { "interruption/recent-dock-during-conversion", REQUEST_RESTORE_DURING_CONVERSION, FALSE, TRUE, FALSE, TRUE, TRUE },
    { "interruption/nested-enumeration", REQUEST_ENUMERATION, FALSE, TRUE, FALSE, TRUE, FALSE },
    { "interruption/mid-fill", REQUEST_MID_FILL, FALSE, TRUE, TRUE, TRUE, FALSE },
    { "interruption/nested-fill", REQUEST_NESTED_FILL, FALSE, TRUE, TRUE, FALSE, FALSE },
    { "save/before-fill-fresh-reader", REQUEST_SAVE_EARLY, FALSE, TRUE, TRUE, TRUE, TRUE },
    { "save/mid-fill-fresh-reader", REQUEST_SAVE_MID_FILL, TRUE, TRUE, FALSE, TRUE, TRUE },
    { "save/clear-suppresses-normal", REQUEST_SAVE_CLEAR, FALSE, TRUE, TRUE, TRUE, FALSE },
    { "save/clear-cancels-deferred-always", REQUEST_SAVE_THEN_CLEAR, FALSE, TRUE, TRUE, TRUE, FALSE },
    { "save/always-after-clear", REQUEST_SAVE_ALWAYS, FALSE, TRUE, TRUE, TRUE, TRUE },
    { "quit/before-fill-fresh-reader", REQUEST_QUIT_EARLY, FALSE, TRUE, TRUE, TRUE, TRUE },
    { "quit/mid-fill-fresh-reader", REQUEST_QUIT_MID_FILL, TRUE, TRUE, FALSE, TRUE, TRUE },
    { "quit/dirty-cancel-then-explicit", REQUEST_DIRTY_CANCEL, FALSE, TRUE, TRUE, TRUE, TRUE }
  };
  gint result;

  g_test_init (&argc, &argv, NULL);
  if (! gtk_init_check (&argc, &argv))
    return GIMP_EXIT_TEST_SKIPPED;
  for (guint i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      gchar *path = g_strconcat ("/painter-session-restore-barrier/", cases[i].name, NULL);
      g_test_add_data_func (path, &cases[i], run_case);
      g_free (path);
    }
  if (! g_test_subprocess ())
    return g_test_run ();

  gimp_test_utils_setup_menus_path ();
  gimp = gimp_init_for_gui_testing (TRUE);
  g_object_set (gimp->config,
                "single-window-mode", g_strcmp0 (g_getenv (ENV_SINGLE), "1") == 0,
                "painter-canvas-ui", g_strcmp0 (g_getenv (ENV_PAINTER), "1") == 0,
                "restore-session", g_strcmp0 (g_getenv (ENV_RESTORE), "1") == 0,
                NULL);
  g_setenv ("GIMP3_DIRECTORY", g_getenv (ENV_PROFILE), TRUE);
  g_signal_connect (gimp, "exit", G_CALLBACK (observe_exit), NULL);
  g_application_run (gimp->app, 0, NULL);
  result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app));
  g_clear_object (&gimp->app);
  return result;
}
