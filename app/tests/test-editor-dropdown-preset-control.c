/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include <string.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimplayerpreset.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpundostack.h"
#include "config/gimpcoreconfig.h"
#include "widgets/gimpeditor.h"
#include "widgets/gimpeditor-cxx.h"
#include "widgets/gimpcontainertreeview.h"
#include "widgets/gimpactionimpl.h"
#include "widgets/gimptoggleaction.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpmenufactory.h"
#include "widgets/gimplayerpresetview.h"
#include "widgets/gimpcontainerview.h"
#include "widgets/gimpaction.h"
#include "widgets/gimpactiongroup.h"
#include "widgets/gimpactionfactory.h"
#include "widgets/gimpdialogfactory.h"
#include "display/display-types.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "actions/layer-presets-actions.h"
#include "menus/menus.h"
#include "dialogs/preferences-dialog.h"
#include "gimpcoreapp.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
static Gimp *gimp;
static void factory_prepare (void)
{
  /* The synthetic dock is not a GimpImageWindow. Initialize the same deferred
   * device lifecycle a real first app focus performs, before preferences/save. */
  gimp_set_focused_once (gimp);
  gchar *path = g_build_filename (g_getenv ("GIMP_TESTING_ABS_TOP_SRCDIR"), "data", "layer-presets", NULL);
  g_object_set (gimp->config, "layer-presets-path", path, NULL); g_free (path);
  gimp_data_factory_data_refresh (gimp->layer_preset_factory, gimp->user_context);
}
static GimpImage *image_new (GimpLayer **source)
{
  GimpImage *image = gimp_image_new (gimp, 64, 64, GIMP_RGB, GIMP_PRECISION_U8_NON_LINEAR);
  *source = gimp_layer_new (image, 64, 64, gimp_image_get_layer_format (image, TRUE), "UI source", 1, GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer (image, *source, NULL, 0, FALSE); gimp_context_set_image (gimp->user_context, image);
  return image;
}
static void select_source (GimpImage *image, GimpLayer *source)
{ GList *layers = g_list_prepend (NULL, source); gimp_image_set_selected_layers (image, layers); g_list_free (layers); }
/* Independent of the new dropdown symbol and common-store controller.
 * The exact same source is runnable before and after the native view fix. */
static void retained_closed_view (void)
{
  GtkWidget *view = gimp_layer_preset_view_new (gimp->user_context, 32,
                                               menus_get_global_menu_factory (gimp));
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (window); g_object_ref_sink (view);
  gtk_container_add (GTK_CONTAINER (window), view); gtk_widget_show_all (window);
  gtk_widget_destroy (window); g_object_unref (window);
  /* No child lease: the view's old child pointer is no longer a valid lookup
   * target. The public retained owner must reject before inspecting children. */
  g_assert_false (gimp_layer_preset_view_can_apply (GIMP_LAYER_PRESET_VIEW (view)));
  gimp_layer_preset_view_apply (GIMP_LAYER_PRESET_VIEW (view));
  g_object_run_dispose (G_OBJECT (view)); g_object_unref (view);
}
static void destroy_from_layer_add (GimpContainer *container, GimpObject *object, gpointer data)
{
  gtk_widget_destroy (GTK_WIDGET (data));
}
static void close_during_apply (void)
{
  GimpLayer *source; GimpImage *image = image_new (&source);
  GtkWidget *view = gimp_layer_preset_view_new (gimp->user_context, 32,
                                               menus_get_global_menu_factory (gimp));
  GtkWidget *window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (window); gtk_container_add (GTK_CONTAINER (window), view); gtk_widget_show_all (window);
  GimpContainerView *container = GIMP_CONTAINER_EDITOR (view)->view;
  GimpContainer *presets = gimp_data_factory_get_container (gimp->layer_preset_factory);
  GimpViewable *preset = GIMP_VIEWABLE (gimp_container_get_child_by_name (presets, "Clone layer"));
  g_assert_nonnull (preset);
  gulong handler = g_signal_connect (gimp_image_get_layers (image), "add", G_CALLBACK (destroy_from_layer_add), window);
  gimp_container_view_set_1_selected (container, preset);
  g_signal_handler_disconnect (gimp_image_get_layers (image), handler);
  g_assert_cmpint (gimp_image_get_n_layers (image), ==, 2);
  g_assert_cmpuint (gimp_image_get_undo_group_count (image), ==, 0);
  g_object_unref (window); gimp_context_set_image (gimp->user_context, NULL); g_object_unref (image);
}

/* This translation unit is a genuine C11 consumer of both exported providers.
 * _Generic rejects a changed parameter, callback-data or variadic signature;
 * the runtime cases below call those declarations, not a C++ test adapter. */
typedef GtkWidget * (*DropdownCSignature) (GimpEditor *, const gchar *,
                                         const gchar *, const gchar *,
                                         const gchar *, GCallback, GCallback,
                                         gpointer);
typedef GtkWidget * (*ActionDropdownCSignature) (GimpEditor *, const gchar *,
                                               const gchar *, const gchar *,
                                               ...);
_Static_assert (_Generic (&gimp_editor_add_dropdown,
                          DropdownCSignature: 1, default: 0),
                "gimp_editor_add_dropdown must preserve its C signature");
_Static_assert (_Generic (&gimp_editor_add_action_dropdown,
                          ActionDropdownCSignature: 1, default: 0),
                "gimp_editor_add_action_dropdown must preserve its C signature");

typedef struct
{
  GtkWidget       *window;
  GtkWidget       *editor;
  GimpMenuFactory *factory;
  gboolean         manager_deleted;
} CFixture;

static void
c_fixture_init (CFixture *fixture)
{
  fixture->factory = menus_get_global_menu_factory (gimp);
  fixture->manager_deleted = FALSE;
  fixture->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (fixture->window);
  fixture->editor = gimp_container_tree_view_new (NULL, gimp->user_context, 32, 0);
  g_object_ref_sink (fixture->editor);
  gimp_editor_create_menu (GIMP_EDITOR (fixture->editor), fixture->factory,
                          "<LayerPresets>", "/layer-presets-popup", fixture->editor);
  g_assert_nonnull (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture->editor)));
  gtk_container_add (GTK_CONTAINER (fixture->window), fixture->editor);
  gtk_widget_show_all (fixture->window);
  gimp_test_run_mainloop_until_idle ();
}

static void
c_fixture_clear (CFixture *fixture)
{
  gtk_widget_destroy (fixture->window);
  if (! fixture->manager_deleted)
    gimp_menu_factory_delete_manager (fixture->factory, "<LayerPresets>",
                                      fixture->editor);
  g_object_unref (fixture->editor);
  g_object_unref (fixture->window);
  gimp_test_run_mainloop_until_idle ();
}

static GtkWidget *
c_assert_dropdown (CFixture *fixture,
                   GtkWidget *button)
{
  GtkBox    *box = gimp_editor_get_button_box (GIMP_EDITOR (fixture->editor));
  GtkWidget *arrow;
  GList     *children;

  g_assert_nonnull (button);
  g_assert_true (GIMP_IS_BUTTON (button));
  g_assert_false (GTK_IS_TOGGLE_BUTTON (button));
  g_assert_false (g_object_is_floating (button));
  g_assert_true (gtk_widget_get_parent (button) == GTK_WIDGET (box));
  children = gtk_container_get_children (GTK_CONTAINER (box));
  g_assert_cmpuint (g_list_length (children), ==, 2);
  g_assert_true (children->data == button);
  arrow = children->next->data;
  g_assert_true (GTK_IS_MENU_BUTTON (arrow));
  g_assert_true (gtk_widget_get_parent (arrow) == GTK_WIDGET (box));
  g_assert_false (g_object_is_floating (arrow));
  g_list_free (children);
  return arrow;
}

typedef struct
{
  guint           cookie;
  guint           normal;
  guint           extended;
  GdkModifierType mask;
  GtkWidget      *button;
  GtkWidget      *destroy;
  gboolean        reenter;
} CBorrowedCalls;

static void
c_generic_clicked (GtkWidget *button,
                   gpointer   data)
{
  CBorrowedCalls *calls = data;

  g_assert_cmphex (calls->cookie, ==, 0xc11cafe);
  g_assert_true (button == calls->button);
  ++calls->normal;
  if (calls->reenter && calls->normal == 1)
    g_signal_emit_by_name (button, "extended-clicked", GDK_CONTROL_MASK);
}

static void
c_generic_extended (GtkWidget       *button,
                    GdkModifierType  mask,
                    gpointer         data)
{
  CBorrowedCalls *calls = data;

  g_assert_cmphex (calls->cookie, ==, 0xc11cafe);
  g_assert_true (button == calls->button);
  ++calls->extended;
  calls->mask = mask;
  if (calls->destroy)
    {
      gtk_widget_destroy (calls->destroy);
      /* A nested invocation after teardown must not call borrowed data again. */
      gtk_button_clicked (GTK_BUTTON (button));
    }
}

static void
c_generic_contract (void)
{
  CFixture        fixture;
  CBorrowedCalls  calls = { .cookie = 0xc11cafe };
  GtkWidget      *button;
  GtkWidget      *arrow;
  GtkWidget      *weak_button;
  GtkWidget      *weak_arrow;
  GtkBox         *box;
  gchar          *tooltip;
  gboolean        expand, fill;
  guint           padding;
  GtkPackType     packing;
  GdkEventScroll  event = { 0 };
  gboolean        handled = FALSE;

  c_fixture_init (&fixture);
  button = gimp_editor_add_dropdown (GIMP_EDITOR (fixture.editor), "document-new",
                                     "C <tooltip>", "c-dropdown-help",
                                     "ignored-list-id", G_CALLBACK (c_generic_clicked),
                                     G_CALLBACK (c_generic_extended), &calls);
  calls.button = button;
  arrow = c_assert_dropdown (&fixture, button);
  box = gimp_editor_get_button_box (GIMP_EDITOR (fixture.editor));
  g_assert_false (gtk_box_get_homogeneous (box));
  gtk_box_query_child_packing (box, button, &expand, &fill, &padding, &packing);
  g_assert_true (expand); g_assert_true (fill);
  g_assert_cmpuint (padding, ==, 0);
  g_assert_cmpint (packing, ==, GTK_PACK_START);
  tooltip = gtk_widget_get_tooltip_text (button);
  g_assert_cmpstr (tooltip, ==, "C <tooltip>"); g_free (tooltip);
  tooltip = gtk_widget_get_tooltip_text (arrow);
  g_assert_cmpstr (tooltip, ==, "C <tooltip>"); g_free (tooltip);
  g_assert_cmpstr (g_object_get_qdata (G_OBJECT (button), GIMP_HELP_ID), ==,
                   "c-dropdown-help");
  g_assert_cmpstr (g_object_get_qdata (G_OBJECT (arrow), GIMP_HELP_ID), ==,
                   "c-dropdown-help");
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_MOD1_MASK);
  g_assert_cmpuint (calls.normal, ==, 1);
  g_assert_cmpuint (calls.extended, ==, 1);
  g_assert_cmpint (calls.mask, ==, GDK_SHIFT_MASK | GDK_MOD1_MASK);
  event.type = GDK_SCROLL;
  g_signal_emit_by_name (arrow, "scroll-event", &event, &handled);
  g_assert_true (handled);

  /* No returned-reference unref: native parenting owns both widgets. */
  weak_button = button; weak_arrow = arrow;
  g_object_add_weak_pointer (G_OBJECT (button), (gpointer *) &weak_button);
  g_object_add_weak_pointer (G_OBJECT (arrow), (gpointer *) &weak_arrow);
  gtk_widget_destroy (fixture.editor);
  g_assert_null (weak_button); g_assert_null (weak_arrow);
  g_assert_cmphex (calls.cookie, ==, 0xc11cafe);
  c_fixture_clear (&fixture);
}

static void
c_generic_optional_callbacks (void)
{
  guint callbacks;

  for (callbacks = 0; callbacks < 4; ++callbacks)
    {
      CFixture       fixture;
      CBorrowedCalls calls = { .cookie = 0xc11cafe };
      GtkWidget     *button;

      c_fixture_init (&fixture);
      button = gimp_editor_add_dropdown (GIMP_EDITOR (fixture.editor), "document-new",
                                         NULL, NULL, NULL,
                                         callbacks & 1 ? G_CALLBACK (c_generic_clicked) : NULL,
                                         callbacks & 2 ? G_CALLBACK (c_generic_extended) : NULL,
                                         &calls);
      calls.button = button;
      c_assert_dropdown (&fixture, button);
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (calls.normal, ==, !! (callbacks & 1));
      g_assert_cmpuint (calls.extended, ==, !! (callbacks & 2));
      c_fixture_clear (&fixture);
      g_assert_cmphex (calls.cookie, ==, 0xc11cafe);
    }
}

static void
c_generic_reentrant_owner_loss (gconstpointer data)
{
  guint           owner = GPOINTER_TO_UINT (data);
  CFixture        fixture;
  CBorrowedCalls  calls = { .cookie = 0xc11cafe, .reenter = TRUE };
  GtkWidget      *button;
  GtkWidget      *arrow;

  c_fixture_init (&fixture);
  button = gimp_editor_add_dropdown (GIMP_EDITOR (fixture.editor), "document-new",
                                     "Reentry", NULL, NULL,
                                     G_CALLBACK (c_generic_clicked),
                                     G_CALLBACK (c_generic_extended), &calls);
  calls.button = button;
  arrow = c_assert_dropdown (&fixture, button);
  calls.destroy = owner == 0 ? fixture.editor : owner == 1 ? button : arrow;
  g_object_ref (button); g_object_ref (arrow);
  gtk_button_clicked (GTK_BUTTON (button));
  g_assert_cmpuint (calls.normal, ==, 1);
  g_assert_cmpuint (calls.extended, ==, 1);
  g_assert_cmpint (calls.mask, ==, GDK_CONTROL_MASK);
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (calls.normal, ==, 1);
  g_assert_cmpuint (calls.extended, ==, 1);
  g_object_unref (button); g_object_unref (arrow);
  c_fixture_clear (&fixture);
  g_assert_cmphex (calls.cookie, ==, 0xc11cafe);
}

static void
c_action_called (GimpAction *action,
                 GVariant   *value,
                 gpointer    data)
{
  ++*((guint *) data);
}

static void
c_toggle_called (GimpToggleAction *action,
                 gpointer          data)
{
  ++*((guint *) data);
}

static GimpActionGroup *
c_action_group (CFixture *fixture)
{
  GimpActionGroup *group = gimp_action_group_new (gimp, "c-dropdown-test", "C test",
                                                 NULL, NULL, NULL);
  gimp_ui_manager_add_action_group (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture->editor)),
                                    group);
  return group;
}

static GimpAction *
c_action_new (GimpActionGroup *group,
              const gchar     *name,
              const gchar     *tooltip,
              gboolean         toggle,
              guint           *count)
{
  GimpAction *action = toggle ?
    gimp_toggle_action_new (name, name, NULL, tooltip, "document-new", "c-action-help",
                            gimp->user_context) :
    gimp_action_impl_new (name, name, NULL, tooltip, "document-new", "c-action-help",
                          gimp->user_context);

  if (toggle)
    g_signal_connect (action, "toggled", G_CALLBACK (c_toggle_called), count);
  else
    g_signal_connect (action, "activate", G_CALLBACK (c_action_called), count);
  gimp_action_group_add_action (group, action);
  g_object_unref (action);
  return action; /* Borrowed from the native action group. */
}

static void
c_action_varargs (gconstpointer data)
{
  guint            variant = GPOINTER_TO_UINT (data);
  gboolean         toggle = variant >= 3;
  guint            pairs = variant % 3;
  CFixture         fixture;
  GimpActionGroup *group;
  GimpAction      *primary;
  GimpAction      *control;
  guint            normal = 0, shift = 0, control_calls = 0, zero = 0;
  GtkWidget       *button;
  GtkWidget       *arrow;
  GtkWidget       *weak_button;
  gchar           *tooltip;

  c_fixture_init (&fixture);
  group = c_action_group (&fixture);
  primary = c_action_new (group, "c-main", "Main <tip>", toggle, &normal);
  c_action_new (group, "c-shift", "Shift <tip>", FALSE, &shift);
  control = c_action_new (group, "c-control", "Control <tip>", FALSE, &control_calls);
  c_action_new (group, "c-zero", "Zero must be ignored", FALSE, &zero);

  /* Each cardinality crosses the exported C varargs ABI with a real NULL
   * pointer sentinel, including pairs that the native helper must skip. */
  if (pairs == 0)
    button = gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), NULL,
                                              "c-dropdown-test", "c-main",
                                              (const gchar *) NULL);
  else if (pairs == 1)
    button = gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), "ignored",
                                              "c-dropdown-test", "c-main",
                                              "c-shift", GDK_SHIFT_MASK,
                                              (const gchar *) NULL);
  else
    button = gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), NULL,
                                              "c-dropdown-test", "c-main",
                                              "c-shift", GDK_SHIFT_MASK,
                                              "c-missing", GDK_MOD1_MASK,
                                              "c-control", GDK_CONTROL_MASK,
                                              "c-zero", (GdkModifierType) 0,
                                              (const gchar *) NULL);
  arrow = c_assert_dropdown (&fixture, button);
  tooltip = gtk_widget_get_tooltip_markup (button);
  g_assert_nonnull (strstr (tooltip, "Main &lt;tip&gt;"));
  if (pairs)
    g_assert_nonnull (strstr (tooltip, "Shift &lt;tip&gt;"));
  if (pairs == 2)
    {
      g_assert_nonnull (strstr (tooltip, "Control &lt;tip&gt;"));
      g_assert_null (strstr (tooltip, "Zero must be ignored"));
    }
  g_free (tooltip);
  tooltip = gtk_widget_get_tooltip_text (arrow);
  g_assert_cmpstr (tooltip, ==, "Main <tip>"); g_free (tooltip);
  g_assert_cmpstr (g_object_get_qdata (G_OBJECT (arrow), GIMP_HELP_ID), ==,
                   "c-action-help");
  g_assert_true ((g_object_get_data (G_OBJECT (button), "extended-actions") != NULL) ==
                 (pairs != 0));
  gtk_button_clicked (GTK_BUTTON (button));
  g_assert_cmpuint (normal, ==, 1);
  if (toggle)
    g_assert_true (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (primary)));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (shift, ==, pairs ? 1 : 0);
  g_signal_emit_by_name (button, "extended-clicked", GDK_MOD1_MASK);
  g_signal_emit_by_name (button, "extended-clicked", (GdkModifierType) 0);
  g_assert_cmpuint (normal, ==, 1);
  g_assert_cmpuint (zero, ==, 0);
  if (pairs == 2)
    {
      /* Native list order gives the later matching pair priority, then falls
       * back to the earlier sensitive match when that action is disabled. */
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
      g_assert_cmpuint (control_calls, ==, 1);
      g_assert_cmpuint (shift, ==, 1);
      gimp_action_set_sensitive (control, FALSE, "C control disabled");
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK | GDK_CONTROL_MASK);
      g_assert_cmpuint (control_calls, ==, 1);
      g_assert_cmpuint (shift, ==, 2);
    }
  gimp_action_set_sensitive (primary, FALSE, "C primary disabled");
  g_assert_false (gtk_widget_get_sensitive (button));
  g_assert_true (gtk_widget_get_sensitive (arrow));
  gimp_action_set_sensitive (primary, TRUE, NULL);
  g_assert_true (gtk_widget_get_sensitive (button));
  if (toggle)
    {
      GtkWidget *ordinary = gimp_editor_add_action_button (GIMP_EDITOR (fixture.editor),
                                                           "c-dropdown-test", "c-main",
                                                           (const gchar *) NULL);
      g_assert_true (GTK_IS_TOGGLE_BUTTON (ordinary));
      gtk_button_clicked (GTK_BUTTON (ordinary));
      g_assert_false (gimp_toggle_action_get_active (GIMP_TOGGLE_ACTION (primary)));
      g_assert_cmpuint (normal, ==, 2);
    }
  weak_button = button;
  g_object_add_weak_pointer (G_OBJECT (button), (gpointer *) &weak_button);
  gtk_widget_destroy (fixture.editor);
  g_assert_null (weak_button); /* The C caller owns no returned reference. */
  g_object_unref (group);
  c_fixture_clear (&fixture);
}

static void
c_action_missing_lookup (void)
{
  CFixture fixture;

  c_fixture_init (&fixture);
  g_assert_null (gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), NULL,
                                                 "missing-c-group", "missing-c-action",
                                                 (const gchar *) NULL));
  {
    GimpActionGroup *group = c_action_group (&fixture);
    g_assert_null (gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), NULL,
                                                   "c-dropdown-test", "missing-c-action",
                                                   (const gchar *) NULL));
    g_object_unref (group);
  }
  g_assert_null (gimp_editor_get_button_box (GIMP_EDITOR (fixture.editor)));
  gimp_menu_factory_delete_manager (fixture.factory, "<LayerPresets>", fixture.editor);
  fixture.manager_deleted = TRUE;
  g_assert_null (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)));
  g_assert_null (gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture.editor), NULL,
                                                 "c-dropdown-test", "missing-c-action",
                                                 (const gchar *) NULL));
  c_fixture_clear (&fixture);
}

static GtkWidget *
c_lifetime_button (CFixture *fixture,
                   gboolean  ordinary)
{
  if (ordinary)
    return gimp_editor_add_action_button (GIMP_EDITOR (fixture->editor),
                                           "c-dropdown-test", "c-main",
                                           "c-alternate", GDK_SHIFT_MASK,
                                           (const gchar *) NULL);
  return gimp_editor_add_action_dropdown (GIMP_EDITOR (fixture->editor), NULL,
                                          "c-dropdown-test", "c-main",
                                          "c-alternate", GDK_SHIFT_MASK,
                                          (const gchar *) NULL);
}

static void
c_action_ownership_order (gconstpointer data)
{
  guint            variant = GPOINTER_TO_UINT (data);
  gboolean         ordinary = variant & 1;
  guint            order = variant / 2;
  CFixture         fixture;
  GimpActionGroup *group;
  GimpAction      *primary;
  GimpAction      *alternate;
  GObject         *weak_alternate;
  GtkWidget       *button;
  guint            normal = 0, modified = 0;

  c_fixture_init (&fixture);
  group = c_action_group (&fixture);
  primary = c_action_new (group, "c-main", "Main", FALSE, &normal);
  alternate = c_action_new (group, "c-alternate", "Alternate", FALSE, &modified);
  weak_alternate = G_OBJECT (alternate);
  g_object_add_weak_pointer (weak_alternate, (gpointer *) &weak_alternate);
  button = c_lifetime_button (&fixture, ordinary);
  g_assert_nonnull (button); g_object_ref (button);
  if (! ordinary)
    c_assert_dropdown (&fixture, button);
  gtk_button_clicked (GTK_BUTTON (button));
  g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
  g_assert_cmpuint (normal, ==, 1);
  g_assert_cmpuint (modified, ==, 1);

  if (order == 0)
    {
      gimp_action_group_remove_action (group, alternate);
      g_assert_null (weak_alternate);
      g_assert_nonnull (gtk_widget_get_parent (button));
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (normal, ==, 2);
      g_assert_cmpuint (modified, ==, 1);
    }
  else if (order == 1)
    {
      GObject *weak_manager = G_OBJECT (gimp_editor_get_ui_manager (GIMP_EDITOR (fixture.editor)));
      GObject *weak_primary = G_OBJECT (primary);
      g_object_add_weak_pointer (weak_manager, (gpointer *) &weak_manager);
      g_object_add_weak_pointer (weak_primary, (gpointer *) &weak_primary);
      g_clear_object (&group);
      gimp_menu_factory_delete_manager (fixture.factory, "<LayerPresets>", fixture.editor);
      fixture.manager_deleted = TRUE;
      g_assert_null (weak_manager); g_assert_null (weak_primary);
      g_assert_null (weak_alternate);
      g_assert_nonnull (gtk_widget_get_parent (button));
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (normal, ==, 1);
      g_assert_cmpuint (modified, ==, 1);
    }
  else
    {
      gtk_widget_destroy (button);
      gimp_action_set_sensitive (primary, FALSE, "Destroyed proxy detached");
      g_assert_true (gtk_widget_get_sensitive (button));
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (normal, ==, 1);
      g_assert_cmpuint (modified, ==, 1);
      /* Destroy and finalize the button before its borrowed alternate. The
       * native list must remove its weak observer, not own or leave it behind. */
      g_clear_object (&button);
      g_assert_nonnull (weak_alternate);
      gimp_action_group_remove_action (group, alternate);
      g_assert_null (weak_alternate);
    }
  g_clear_object (&button);
  g_clear_object (&group);
  c_fixture_clear (&fixture);
}

typedef struct
{
  GimpActionGroup *group;
  GtkWidget      *button;
  gboolean        destroy;
  guint           calls;
} CRemoveAlternate;

static void
c_remove_reenter (GimpAction *action,
                  GVariant   *value,
                  gpointer    data)
{
  CRemoveAlternate *state = data;

  ++state->calls;
  gimp_action_set_sensitive (action, FALSE, "C alternate removing itself");
  gimp_action_group_remove_action (state->group, action);
  if (state->destroy)
    gtk_widget_destroy (state->button);
  g_signal_emit_by_name (state->button, "extended-clicked", GDK_SHIFT_MASK);
}

static void
c_action_reentrant_removal (gconstpointer data)
{
  gboolean ordinary = GPOINTER_TO_UINT (data);
  guint    destroy;

  for (destroy = 0; destroy < 2; ++destroy)
    {
      CFixture         fixture;
      GimpActionGroup *group;
      GimpAction      *alternate;
      GObject         *weak_alternate;
      GtkWidget       *button;
      guint            normal = 0, modified = 0;
      CRemoveAlternate state;

      c_fixture_init (&fixture);
      group = c_action_group (&fixture);
      c_action_new (group, "c-main", "Main", FALSE, &normal);
      alternate = c_action_new (group, "c-alternate", "Alternate", FALSE, &modified);
      weak_alternate = G_OBJECT (alternate);
      g_object_add_weak_pointer (weak_alternate, (gpointer *) &weak_alternate);
      button = c_lifetime_button (&fixture, ordinary);
      g_assert_nonnull (button); g_object_ref (button);
      state = (CRemoveAlternate) { group, button, destroy != 0, 0 };
      g_signal_connect (alternate, "activate", G_CALLBACK (c_remove_reenter), &state);
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (modified, ==, 1);
      g_assert_cmpuint (state.calls, ==, 1);
      g_assert_null (weak_alternate);
      gtk_button_clicked (GTK_BUTTON (button));
      g_signal_emit_by_name (button, "extended-clicked", GDK_SHIFT_MASK);
      g_assert_cmpuint (normal, ==, destroy ? 1 : 2);
      g_assert_cmpuint (modified, ==, 1);
      g_assert_cmpuint (state.calls, ==, 1);
      g_object_unref (button); g_object_unref (group);
      c_fixture_clear (&fixture);
    }
}

int main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL); if (!gtk_init_check (&argc, &argv)) return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path (); gimp = gimp_init_for_gui_testing (TRUE); factory_prepare ();
  g_test_add_func ("/editor-dropdown-preset-control/retained-closed-view", retained_closed_view);
  g_test_add_func ("/editor-dropdown-preset-control/close-during-apply", close_during_apply);
  g_test_add_func ("/editor-dropdown-preset-control/c-generic-contract", c_generic_contract);
  g_test_add_func ("/editor-dropdown-preset-control/c-generic-optional-callbacks", c_generic_optional_callbacks);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-generic-reentry-editor", GUINT_TO_POINTER (0), c_generic_reentrant_owner_loss);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-generic-reentry-primary", GUINT_TO_POINTER (1), c_generic_reentrant_owner_loss);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-generic-reentry-arrow", GUINT_TO_POINTER (2), c_generic_reentrant_owner_loss);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-normal-zero-pairs", GUINT_TO_POINTER (0), c_action_varargs);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-normal-one-pair", GUINT_TO_POINTER (1), c_action_varargs);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-normal-multiple-pairs", GUINT_TO_POINTER (2), c_action_varargs);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-toggle-zero-pairs", GUINT_TO_POINTER (3), c_action_varargs);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-toggle-one-pair", GUINT_TO_POINTER (4), c_action_varargs);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-toggle-multiple-pairs", GUINT_TO_POINTER (5), c_action_varargs);
  g_test_add_func ("/editor-dropdown-preset-control/c-action-missing-lookup", c_action_missing_lookup);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-alternate-first-dropdown", GUINT_TO_POINTER (0), c_action_ownership_order);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-alternate-first-ordinary", GUINT_TO_POINTER (1), c_action_ownership_order);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-manager-first-dropdown", GUINT_TO_POINTER (2), c_action_ownership_order);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-manager-first-ordinary", GUINT_TO_POINTER (3), c_action_ownership_order);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-button-first-dropdown", GUINT_TO_POINTER (4), c_action_ownership_order);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-button-first-ordinary", GUINT_TO_POINTER (5), c_action_ownership_order);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-reentrant-removal-dropdown", GUINT_TO_POINTER (0), c_action_reentrant_removal);
  g_test_add_data_func ("/editor-dropdown-preset-control/c-action-reentrant-removal-ordinary", GUINT_TO_POINTER (1), c_action_reentrant_removal);
  g_application_run (gimp->app, 0, NULL);
  gint result = gimp_core_app_get_exit_status (GIMP_CORE_APP (gimp->app));
  g_application_quit (G_APPLICATION (gimp->app)); g_clear_object (&gimp->app); return result;
}
