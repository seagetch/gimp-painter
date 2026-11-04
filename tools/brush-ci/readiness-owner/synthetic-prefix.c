/* Minimal GLib ABI declarations for headerless ordinary runtime proof only. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef unsigned guint; typedef unsigned long gulong,GType; typedef int gboolean; typedef int64_t gint64; typedef char gchar; typedef void *gpointer;
typedef struct {GType g_type;} GTypeClass;
typedef struct {GTypeClass *g_class;} GObject;
typedef GObject GtkWidget; typedef GObject GdkFrameClock;
typedef struct GList {void *data;struct GList *next,*prev;} GList;
typedef struct {GType g_type;union {int v_int;unsigned v_uint;long v_long;unsigned long v_ulong;int64_t v_int64;uint64_t v_uint64;float v_float;double v_double;void *v_pointer;} data[2];} GValue;
typedef struct {guint signal_id;guint detail;guint run_type;} GSignalInvocationHint;
typedef struct {guint signal_id;const char *signal_name;GType itype;guint signal_flags;GType return_type;guint n_params;const GType *param_types;} GSignalQuery;
#define TRUE 1
#define FALSE 0
#define G_TYPE_NONE 4
#define G_TYPE_OBJECT 80
#define G_SIGNAL_NO_HOOKS (1<<6)
#define G_OBJECT(x) ((GObject*)(x))
#define G_OBJECT_TYPE(x) (((GObject*)(x))->g_class->g_type)
#define G_OBJECT_TYPE_NAME(x) ("GObject")
#define G_CALLBACK(x) ((void(*)(void))(x))
#define G_GINT64_FORMAT "ld"
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))
extern gint64 g_get_monotonic_time(void); extern void *g_value_get_object(const GValue*);
extern void g_object_add_weak_pointer(void*,void**);extern void g_object_remove_weak_pointer(void*,void**);
extern guint g_signal_lookup(const char*,GType);extern void g_signal_query(guint,GSignalQuery*);
extern gulong g_signal_add_emission_hook(guint,guint,gboolean(*)(GSignalInvocationHint*,guint,const GValue*,void*),void*,void(*)(void*));
extern void g_signal_remove_emission_hook(guint,gulong);
extern gulong g_signal_connect_data(void*,const char*,void(*)(void),void*,void*,guint);
#define g_signal_connect_after(o,n,c,d) g_signal_connect_data(o,n,c,d,NULL,1)
extern void g_signal_handler_disconnect(void*,gulong);
extern GList *g_list_prepend(GList*,void*);extern GList *g_list_remove(GList*,const void*);extern void g_list_free(GList*);
extern void g_test_init(int*,char***,...);
extern int g_str_equal(const void*,const void*);extern void g_print(const char*,...);
extern void *g_object_new(GType,const char*,...);extern void g_object_unref(void*);
extern guint g_signal_new(const char*,GType,guint,guint,void*,void*,void*,GType,guint,...);
extern void g_signal_emit_by_name(void*,const char*,...);
static GtkWidget *objects[20];static GdkFrameClock *clocks[2];
static GtkWidget *gtk_widget_get_toplevel(GtkWidget *w){return w;}
static GList *gtk_window_list_toplevels(void){GList *l=NULL;for(int i=19;i>=0;i--)l=g_list_prepend(l,objects[i]);return l;}
static gboolean gtk_widget_get_mapped(GtkWidget *w){(void)w;return TRUE;}
static gboolean gtk_widget_get_realized(GtkWidget *w){(void)w;return TRUE;}
static GdkFrameClock *gtk_widget_get_frame_clock(GtkWidget *w){return w==objects[0]?clocks[0]:clocks[1];}
