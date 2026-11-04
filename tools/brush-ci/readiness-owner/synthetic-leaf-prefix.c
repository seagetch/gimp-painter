/* Headerless real-GObject proof. These are synthetic method-slot types, NOT
 * declarations of the GTK ABI and NOT a native GTK/GUI test. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned guint;
typedef unsigned long GType;
typedef int gboolean, gint;
typedef int64_t gint64;
typedef char gchar;
typedef void *gpointer;
typedef struct { GType g_type; } GTypeClass;
typedef struct { GTypeClass *g_class; guint ref_count; gpointer qdata; } GObject;
typedef struct TestWidget { GObject object; struct TestWidget *parent; } GtkWidget;
typedef GObject GtkCellRenderer;
typedef struct { gint x, y, width, height; } GdkRectangle;
typedef void (*ImageWidth) (GtkWidget *, gint *, gint *);
typedef void (*TextWidth) (GtkCellRenderer *, GtkWidget *, gint *, gint *);
typedef void (*PixbufSize) (GtkCellRenderer *, GtkWidget *, const GdkRectangle *,
                             gint *, gint *, gint *, gint *);
/* Reserve exceeds real GObjectClass, checked using g_type_query below. */
typedef struct {
  GTypeClass parent; gpointer reserve[32];
  ImageWidth get_preferred_width, get_preferred_height, baseline;
} GtkWidgetClass;
typedef struct {
  GTypeClass parent; gpointer reserve[32];
  TextWidth get_preferred_width, get_preferred_height;
  PixbufSize get_size;
} GtkCellRendererClass;
typedef struct { GType type; const char *name; guint class_size, instance_size; } GTypeQuery;
typedef struct {
  uint16_t class_size;
  void (*base_init) (gpointer), (*base_finalize) (gpointer);
  void (*class_init) (gpointer, gpointer), (*class_finalize) (gpointer, gpointer);
  const void *class_data;
  uint16_t instance_size, n_preallocs;
  void (*instance_init) (gpointer, gpointer);
  const void *value_table;
} GTypeInfo;
typedef struct {
  void (*interface_init) (gpointer, gpointer), (*interface_finalize) (gpointer, gpointer);
  gpointer interface_data;
} GInterfaceInfo;
typedef struct {
  GType type, instance_type;
  void (*use_plugin) (gpointer), (*unuse_plugin) (gpointer);
  void (*complete_type_info) (gpointer, GType, GTypeInfo *, gpointer);
  void (*complete_interface_info) (gpointer, GType, GType, GInterfaceInfo *);
} TestPluginInterface;
#define TRUE 1
#define FALSE 0
#define G_TYPE_OBJECT 80
#define G_OBJECT(x) ((GObject *) (x))
#define G_OBJECT_TYPE(x) (G_OBJECT (x)->g_class->g_type)
#define G_GINT64_FORMAT "ld"
extern GType g_type_from_name (const gchar *);
extern gpointer g_type_class_peek (GType);
extern gpointer g_type_class_ref (GType);
extern void g_type_class_unref (gpointer);
extern gpointer g_type_get_plugin (GType);
extern gboolean g_type_is_a (GType, GType);
extern GType *g_type_children (GType, guint *);
extern void g_free (gpointer);
extern void g_type_query (GType, GTypeQuery *);
extern GType g_type_register_static_simple (GType, const gchar *, guint,
                                            void (*) (gpointer, gpointer), guint,
                                            void (*) (gpointer, gpointer), guint);
extern gpointer g_object_new (GType, const gchar *, ...);
extern void g_object_unref (gpointer);
extern void g_object_add_weak_pointer (gpointer, gpointer *);
extern void g_object_remove_weak_pointer (gpointer, gpointer *);
extern GType g_type_plugin_get_type (void);
extern void g_type_add_interface_static (GType, GType, const GInterfaceInfo *);
extern GType g_type_register_dynamic (GType, const gchar *, gpointer, guint);
static GType dynamic_type;
static guint plugin_uses;
static gint64 fake_time = 100;
static gint64 fake_clock (void) { return ++fake_time; }
#define g_get_monotonic_time fake_clock
static GtkWidget *gtk_widget_get_parent (GtkWidget *w) { return w->parent; }
static GtkWidget *gtk_widget_get_toplevel (GtkWidget *w)
{ while (w->parent) w = w->parent; return w; }
static GType image_type, text_type, pixbuf_type;
static GtkWidget *target_window, *harness_window, *image_object, *original_window;
static GtkCellRenderer *text_object, *pixbuf_object;
static guint original_calls[3], override_calls, recurse, nested;
static guint clock_backwards, omit_owner;
static const GdkRectangle *expected_area;
static void (*cached_image) (GtkWidget *, gint *, gint *);
static void original_image (GtkWidget *, gint *, gint *);
static void original_text (GtkCellRenderer *, GtkWidget *, gint *, gint *);
static void original_pixbuf (GtkCellRenderer *, GtkWidget *, const GdkRectangle *,
                             gint *, gint *, gint *, gint *);
static void inert_image (GtkWidget *w, gint *a, gint *b) { (void) w; (void) a; (void) b; }
static void inert_text (GtkCellRenderer *c, GtkWidget *w, gint *a, gint *b)
{ (void) c; (void) w; (void) a; (void) b; }
static void image_init (gpointer klass, gpointer data)
{
  GtkWidgetClass *k = klass; (void) data;
  k->get_preferred_width = original_image;
  k->get_preferred_height = inert_image; k->baseline = inert_image;
}
static void text_init (gpointer klass, gpointer data)
{
  GtkCellRendererClass *k = klass; (void) data;
  k->get_preferred_width = original_text; k->get_preferred_height = inert_text;
}
static void pixbuf_init (gpointer klass, gpointer data)
{
  GtkCellRendererClass *k = klass; (void) data;
  k->get_size = original_pixbuf; k->get_preferred_height = inert_text;
}
static void override_image (GtkWidget *w, gint *a, gint *b)
{
  ++override_calls;
  ((GtkWidgetClass *) g_type_class_peek (image_type))->get_preferred_width (w, a, b);
}
static void override_init (gpointer klass, gpointer data)
{ (void) data; ((GtkWidgetClass *) klass)->get_preferred_width = override_image; }
static void null_init (gpointer klass, gpointer data)
{ image_init (klass, data); ((GtkWidgetClass *) klass)->get_preferred_width = NULL; }
static void original_image (GtkWidget *w, gint *minimum, gint *natural)
{
  ++original_calls[0]; fake_time += 10;
  if (clock_backwards) { clock_backwards = 0; fake_time -= 1000; }
  if (recurse) {
    recurse = 0;
    ((GtkWidgetClass *) G_OBJECT (w)->g_class)->get_preferred_width (w, NULL, NULL);
  }
  if (nested) {
    nested = 0;
    ((GtkCellRendererClass *) G_OBJECT (text_object)->g_class)->get_preferred_width
      (text_object, w, NULL, NULL);
    ((GtkCellRendererClass *) G_OBJECT (pixbuf_object)->g_class)->get_size
      (pixbuf_object, w, expected_area, NULL, NULL, NULL, NULL);
  }
  if (minimum) *minimum = 11;
  if (natural) *natural = 12;
}
static void original_text (GtkCellRenderer *cell, GtkWidget *w, gint *minimum, gint *natural)
{
  assert (cell == text_object); (void) w;
  ++original_calls[1]; fake_time += 20;
  if (minimum) *minimum = 41;
  if (natural) *natural = 42;
}
static void original_pixbuf (GtkCellRenderer *cell, GtkWidget *w, const GdkRectangle *area,
                             gint *x, gint *y, gint *width, gint *height)
{
  assert (cell == pixbuf_object); assert (area == expected_area); (void) w;
  ++original_calls[2]; fake_time += 30;
  if (x) *x = 21;
  if (y) *y = 22;
  if (width) *width = 23;
  if (height) *height = 24;
}
static GType new_image_type (const char *name, GType parent,
                            void (*init) (gpointer, gpointer), gboolean initialize)
{
  GType t = g_type_register_static_simple (parent, name, sizeof (GtkWidgetClass), init,
                                          sizeof (GtkWidget), NULL, 0);
  assert (t);
  if (initialize) assert (g_type_class_ref (t));
  return t;
}
static GType new_renderer_type (const char *name, GType parent)
{
  GType type = g_type_register_static_simple (parent, name, sizeof (GtkCellRendererClass),
                                             NULL, sizeof (GObject), NULL, 0);
  assert (type && g_type_class_ref (type));
  return type;
}
static void plugin_use (gpointer plugin) { (void) plugin; ++plugin_uses; }
static void plugin_unuse (gpointer plugin) { (void) plugin; }
static void plugin_complete (gpointer plugin, GType type, GTypeInfo *info, gpointer values)
{
  (void) plugin; (void) type; (void) values;
  memset (info, 0, sizeof *info);
  info->class_size = sizeof (GtkWidgetClass);
  info->instance_size = sizeof (GtkWidget);
}
static void plugin_interface_init (gpointer iface, gpointer data)
{
  TestPluginInterface *i = iface; (void) data;
  i->use_plugin = plugin_use; i->unuse_plugin = plugin_unuse;
  i->complete_type_info = plugin_complete;
}
static GType new_dynamic_image (const char *name, GType parent)
{
  static gpointer plugin;
  if (!plugin) {
    GInterfaceInfo info = { plugin_interface_init, NULL, NULL };
    GType p = g_type_register_static_simple (G_TYPE_OBJECT, "SyntheticLeafPlugin",
      sizeof (GtkWidgetClass), NULL, sizeof (GObject), NULL, 0);
    g_type_add_interface_static (p, g_type_plugin_get_type (), &info);
    plugin = g_object_new (p, NULL);
  }
  GType type = g_type_register_dynamic (parent, name, plugin, 0);
  assert (type && g_type_get_plugin (type) == plugin);
  assert (g_type_class_ref (type));
  assert (plugin_uses > 0);
  return type;
}
static void setup (gboolean null_base)
{
  GTypeQuery q;
  g_type_query (G_TYPE_OBJECT, &q);
  assert (q.class_size <= sizeof (GTypeClass) + sizeof (gpointer) * 32);
  assert (q.instance_size <= sizeof (GObject));
  image_type = new_image_type ("GtkImage", G_TYPE_OBJECT, null_base ? null_init : image_init, TRUE);
  text_type = g_type_register_static_simple (G_TYPE_OBJECT, "GtkCellRendererText",
    sizeof (GtkCellRendererClass), text_init, sizeof (GObject), NULL, 0);
  pixbuf_type = g_type_register_static_simple (G_TYPE_OBJECT, "GtkCellRendererPixbuf",
    sizeof (GtkCellRendererClass), pixbuf_init, sizeof (GObject), NULL, 0);
  assert (g_type_class_ref (text_type)); assert (g_type_class_ref (pixbuf_type));
  if (!omit_owner) {
    GType window_type = new_image_type ("GimpImageWindow", G_TYPE_OBJECT, image_init, TRUE);
    original_window = g_object_new (window_type, NULL);
  }
  target_window = g_object_new (image_type, NULL);
  harness_window = g_object_new (image_type, NULL);
  image_object = g_object_new (image_type, NULL); image_object->parent = target_window;
  text_object = g_object_new (text_type, NULL);
  pixbuf_object = g_object_new (pixbuf_type, NULL);
}

/* C42 profiler is tested independently with real weak lifetimes. This slot-only
 * proof keeps its no-op interface so exact leaf forwarding remains isolated. */
typedef struct { guint unused; } ImageCall;
static ImageCall image_observe_enter (GtkWidget *widget)
{ (void) widget; return (ImageCall) { 0 }; }
static void image_observe_exit (ImageCall call) { (void) call; }

/* C43 independent icon profile is tested by its own real-GObject harness. */
typedef struct { int unused; } IconCall;
static IconCall icon_observe_enter (GtkWidget *widget) { (void) widget; return (IconCall) {0}; }
static void icon_observe_exit (IconCall call) { (void) call; }
