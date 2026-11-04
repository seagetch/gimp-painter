/* Real GObject lifecycle + synthetic GTK getter/container signatures.
 * These structs are NOT the GTK ABI; no native GTK or GUI is executed. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned guint;
typedef unsigned long gulong, GType;
typedef int gint, gboolean, GtkIconSize;
typedef int64_t gint64;
typedef char gchar;
typedef void *gpointer;
typedef struct { GType g_type; } GTypeClass;
typedef struct { GTypeClass *g_class; guint ref_count; gpointer qdata; } GObject;
typedef struct TestWidget {
  GObject object;
  struct TestWidget *parent, **children, **internal;
  guint children_count, internal_count;
  guint storage, state;
  gint logical, pixel, scale;
  gboolean visible, mapped, child_visible, realized;
} GtkWidget;
typedef GtkWidget GtkImage, GtkContainer;
typedef struct { GTypeClass parent; gpointer reserve[32]; } TestClass;
typedef struct { GType type; const gchar *name; guint class_size, instance_size; } GTypeQuery;
typedef struct { guint signal_id; const gchar *signal_name; GType itype;
                 guint signal_flags; GType return_type; guint n_params;
                 const GType *param_types; } GSignalQuery;
typedef struct GList { gpointer data; struct GList *next, *prev; } GList;
#define TRUE 1
#define FALSE 0
#define G_TYPE_OBJECT 80
#define G_TYPE_NONE 4
#define G_OBJECT(x) ((GObject *) (x))
#define G_OBJECT_TYPE(x) (G_OBJECT (x)->g_class->g_type)
#define GTK_IMAGE(x) ((GtkImage *) (x))
#define GTK_CONTAINER(x) ((GtkContainer *) (x))
#define G_CALLBACK(x) ((void (*) (void)) (x))
#define G_GINT64_FORMAT "ld"
#define G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#define G_GNUC_END_IGNORE_DEPRECATIONS
#define GTK_ICON_SIZE_INVALID 0
#define GTK_IMAGE_EMPTY 0u
#define GTK_IMAGE_PIXBUF 1u
#define GTK_IMAGE_STOCK 2u
#define GTK_IMAGE_ICON_SET 3u
#define GTK_IMAGE_ANIMATION 4u
#define GTK_IMAGE_ICON_NAME 5u
#define GTK_IMAGE_GICON 6u
#define GTK_IMAGE_SURFACE 7u
extern GType g_type_from_name (const gchar *);
extern gboolean g_type_is_a (GType, GType);
extern gpointer g_type_class_ref (GType);
extern void g_type_query (GType, GTypeQuery *);
extern GType g_type_register_static_simple (GType, const gchar *, guint,
                                            void (*) (gpointer, gpointer), guint,
                                            void (*) (gpointer, gpointer), guint);
extern gpointer g_object_new (GType, const gchar *, ...);
extern void g_object_unref (gpointer);
extern void g_object_add_weak_pointer (gpointer, gpointer *);
extern void g_object_remove_weak_pointer (gpointer, gpointer *);
extern guint g_signal_new (const gchar *, GType, guint, guint, gpointer, gpointer,
                          gpointer, GType, guint, ...);
extern guint g_signal_lookup (const gchar *, GType);
extern void g_signal_query (guint, GSignalQuery *);
extern gulong g_signal_connect_data (gpointer, const gchar *, void (*) (void),
                                     gpointer, gpointer, guint);
extern gboolean g_signal_handler_is_connected (gpointer, gulong);
extern void g_signal_handler_disconnect (gpointer, gulong);
extern void g_signal_emit_by_name (gpointer, const gchar *, ...);
extern void g_list_free (GList *);
extern GList *g_list_append (GList *, gpointer);
static gboolean in_forward, forwarding_destroyed;
static guint weak_adds, weak_removes, getter_calls, originals, forall_calls;
static void add_weak (gpointer object, gpointer *location)
{ assert (!in_forward); ++weak_adds; g_object_add_weak_pointer (object, location); }
static void remove_weak (gpointer object, gpointer *location)
{ assert (!in_forward); ++weak_removes; g_object_remove_weak_pointer (object, location); }
#define g_object_add_weak_pointer add_weak
#define g_object_remove_weak_pointer remove_weak
#define g_signal_connect(o,n,f,d) g_signal_connect_data ((o),(n),(f),(d),NULL,0)
static gint64 fake_time = 100;
static gint64 fake_clock (void) { return ++fake_time; }
#define g_get_monotonic_time fake_clock
static GtkWidget *roots[256];
static guint root_count;
static GtkWidget *reuse_address, *reuse_object;
static GtkWidget *mutation_container, *mutation_victim;
static gboolean finalize_queued;
static GtkWidget *resolve (GtkWidget *widget)
{
  assert (!forwarding_destroyed); /* forbidden post-original widget access */
  ++getter_calls;
  if (widget == reuse_address) return reuse_object;
  return widget;
}
static GList *gtk_window_list_toplevels (void)
{
  GList *list = NULL;
  for (guint i = 0; i < root_count; ++i) list = g_list_append (list, roots[i]);
  return list;
}
static void gtk_container_forall (GtkContainer *container,
                                 void (*callback) (GtkWidget *, gpointer), gpointer data)
{
  ++forall_calls;
  if (container == mutation_container) {
    mutation_container = NULL;
    if (finalize_queued) g_object_unref (mutation_victim);
    else mutation_victim->parent = container;
  }
  for (guint i = 0; i < container->children_count; ++i) callback (container->children[i], data);
  /* Internal children model notebook tab labels; foreach would miss these. */
  for (guint i = 0; i < container->internal_count; ++i) callback (container->internal[i], data);
}
static GtkWidget *gtk_widget_get_parent (GtkWidget *widget) { return resolve (widget)->parent; }
static guint gtk_image_get_storage_type (GtkImage *image) { return resolve (image)->storage; }
static void get_logical (GtkImage *image, gpointer output, GtkIconSize *size, guint storage)
{ image = resolve (image); assert (!output && image->storage == storage); *size = image->logical; }
static void gtk_image_get_icon_name (GtkImage *image, const gchar **name, GtkIconSize *size)
{ get_logical (image, name, size, GTK_IMAGE_ICON_NAME); }
static void gtk_image_get_gicon (GtkImage *image, gpointer icon, GtkIconSize *size)
{ get_logical (image, icon, size, GTK_IMAGE_GICON); }
static void gtk_image_get_stock (GtkImage *image, gpointer stock, GtkIconSize *size)
{ get_logical (image, stock, size, GTK_IMAGE_STOCK); }
static void gtk_image_get_icon_set (GtkImage *image, gpointer set, GtkIconSize *size)
{ get_logical (image, set, size, GTK_IMAGE_ICON_SET); }
static gint gtk_image_get_pixel_size (GtkImage *image) { return resolve (image)->pixel; }
static gboolean gtk_widget_get_visible (GtkWidget *widget) { return resolve (widget)->visible; }
static gboolean gtk_widget_get_mapped (GtkWidget *widget) { return resolve (widget)->mapped; }
static gboolean gtk_widget_get_child_visible (GtkWidget *widget) { return resolve (widget)->child_visible; }
static gboolean gtk_widget_get_realized (GtkWidget *widget) { return resolve (widget)->realized; }
static guint gtk_widget_get_state_flags (GtkWidget *widget) { return resolve (widget)->state; }
static gint gtk_widget_get_scale_factor (GtkWidget *widget)
{
  widget = resolve (widget);
  if (!widget->realized) {
    GtkWidget *parent = widget;
    guint visited = 0;
    while (parent->parent) { assert (++visited < 128); parent = parent->parent; }
  }
  return widget->scale;
}
static GType widget_type, container_type, image_type, window_type, semantic_types[5];
static GtkWidget *window_object, *tool_button, *first_image, *internal_image, *uncalled_image;
static GType new_type (const gchar *name, GType parent)
{
  GType type = g_type_register_static_simple (parent, name, sizeof (TestClass), NULL,
                                             sizeof (GtkWidget), NULL, 0);
  assert (type && g_type_class_ref (type)); return type;
}
static GtkWidget *new_widget (GType type)
{
  GtkWidget *widget = g_object_new (type, NULL);
  widget->storage = GTK_IMAGE_ICON_NAME; widget->logical = 4; widget->pixel = -1;
  widget->visible = widget->mapped = widget->child_visible = widget->realized = TRUE;
  widget->scale = 1; return widget;
}
static void add_child (GtkWidget *parent, GtkWidget *child, gboolean internal)
{
  GtkWidget ***list = internal ? &parent->internal : &parent->children;
  guint *count = internal ? &parent->internal_count : &parent->children_count;
  *list = realloc (*list, sizeof (GtkWidget *) * (*count + 1)); assert (*list);
  (*list)[(*count)++] = child; child->parent = parent;
}
static void setup (const gchar *mode)
{
  const gchar *names[5] = {"GtkToolButton", "GimpItemTreeView", "GimpEditor", "GimpDockbook", "GimpDisplayShell"};
  GTypeQuery query;
  g_type_query (G_TYPE_OBJECT, &query);
  assert (query.class_size <= sizeof (TestClass));
  assert (query.instance_size <= sizeof (GObject));
  widget_type = new_type ("SyntheticImageWidget", G_TYPE_OBJECT);
  if (strcmp (mode, "missing-destroy"))
    assert (g_signal_new ("destroy", widget_type, 2, 0, NULL, NULL, NULL, G_TYPE_NONE, 0));
  container_type = new_type ("GtkContainer", widget_type);
  image_type = new_type ("GtkImage", widget_type);
  window_type = new_type (strcmp (mode, "missing-type") ? "GimpImageWindow" : "MissingWindow", container_type);
  semantic_types[2] = new_type (names[2], container_type);
  for (guint i = 0; i < 5; ++i) {
    if (i == 2) continue;
    semantic_types[i] = new_type (!strcmp (mode, "missing-semantic") && !i ? "MissingToolButton" : names[i],
                                 i == 1 ? semantic_types[2] : container_type);
  }
  window_object = new_widget (window_type); roots[root_count++] = window_object;
  tool_button = new_widget (semantic_types[0]); add_child (window_object, tool_button, FALSE);
  first_image = new_widget (image_type); add_child (tool_button, first_image, FALSE);
  internal_image = new_widget (image_type); add_child (tool_button, internal_image, TRUE);
  uncalled_image = new_widget (image_type); add_child (window_object, uncalled_image, FALSE);
  /* Duplicate exposure must not create a second identity or alter real parent. */
  window_object->internal = malloc (sizeof (GtkWidget *)); assert (window_object->internal);
  window_object->internal[0] = internal_image; window_object->internal_count = 1;
  if (!strcmp (mode, "queue-finalized") || !strcmp (mode, "queue-reparented")) {
    mutation_container = tool_button; mutation_victim = uncalled_image;
    finalize_queued = !strcmp (mode, "queue-finalized");
  }
}
