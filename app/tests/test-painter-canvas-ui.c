/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpbrush.h"
#include "core/gimptempbuf.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer-new.h"
#include "core/gimpgrouplayer.h"
#include "core/gimpclonelayer.h"
#include "core/gimplayermask.h"
#include "core/gimptoolinfo.h"
#include "core/gimptoolgroup.h"
#include "core/gimptooloptions.h"
#include "core/gimpcontainer.h"
#include "widgets/gimppainterlayertiles.h"
#include "widgets/gimpview.h"
#include "widgets/gimpviewrenderer.h"
#include "widgets/gimplayermodebox.h"
#include "widgets/gimpoverlaybox.h"
#include "widgets/gimpoverlaychild.h"
#include "widgets/gimpcoloreditor.h"
#include "widgets/gimpfgbgeditor.h"
#include "widgets/gimpwidgets-utils.h"
#include "widgets/gimpuimanager.h"
#include "widgets/gimpdocked.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimpimagewindow.h"
#include "display/gimppaintercanvasui.h"
#include "painter/gimp-painter-binding.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
#include "gimpcoreapp.h"
#include <string.h>
#include "widgets/gimpdnd.h"
#include "widgets/gimpselectiondata.h"
#include "core/gimpdrawable.h"
#include "core/gimp-utils.h"
#include "widgets/gimpviewrendererdrawable.h"
#include "libgimpconfig/gimpconfig.h"
#include "gui/session.h"
#include "core/gimplist.h"
#include "dialogs/dialogs.h"
#include "widgets/gimpsessioninfo.h"

static Gimp *gimp;
static GtkWidget *window;
typedef struct { void (*run) (void); } CanvasTestCase;
static void run_canvas_case (gconstpointer data) {
  /* Device restoration is deferred until the first focus. Each -p case must
   * establish it itself, rather than depend on an earlier case/window focus. */
  gimp_set_focused_once(gimp);
  ((const CanvasTestCase *) data)->run();
}
static void drain (void) { for (int i=0;i<300 && g_main_context_pending(NULL);++i) g_main_context_iteration(NULL,FALSE); }
static GtkWidget *find (GtkWidget *w,const gchar *name) {
  if (!g_strcmp0(gtk_widget_get_name(w),name)) return w;
  if (GTK_IS_CONTAINER(w)) {
    GList *children=gtk_container_get_children(GTK_CONTAINER(w));
    GtkWidget *result=NULL;
    for(GList *i=children;i && !result;i=i->next)
      result=find(i->data,name);
    g_list_free(children);
    return result;
  }
  return NULL;
}
static GimpImage *new_image(void) { return gimp_image_new(gimp,64,64,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR); }
static GimpLayer *layer(GimpImage *image,GimpLayer *parent,const char *name) {
  GimpLayer *result=gimp_layer_new(image,64,64,gimp_image_get_layer_format(image,TRUE),name,1,GIMP_LAYER_MODE_NORMAL_LEGACY);
  gimp_image_add_layer(image,result,parent,0,FALSE); return result;
}
static GtkWidget *tiles(GimpImage *image) {
  GtkWidget *w=gimp_painter_layer_tiles_new(gimp_get_user_context(gimp),image);
  gtk_container_add(GTK_CONTAINER(window),w); gtk_widget_show_all(window); drain(); return w;
}
static void close_tiles(GtkWidget *w) { gtk_widget_destroy(w); drain(); }
static void channel_preview_components(void) {
  const gchar *formats[]={"R~G~B~A u8","Y~A u8"};
  for(guint format=0;format<G_N_ELEMENTS(formats);++format) {
    const Babl *babl=babl_format(formats[format]);
    gint bytes=babl_format_get_bytes_per_pixel(babl);
    GimpTempBuf *buffer=gimp_temp_buf_new(2,1,babl);
    guchar *data=gimp_temp_buf_get_data(buffer);
    for(int x=0;x<2;++x) {
      for(int c=0;c<bytes-1;++c)data[x*bytes+c]=x?90:50;
      data[x*bytes+bytes-1]=x?200:32;
    }
    GimpViewRenderer *renderer=gimp_view_renderer_new_full(gimp_get_user_context(gimp),GIMP_TYPE_IMAGE,2,1,0,FALSE);
    for(int component=0;component<2;++component) {
      gint channel=component?bytes-1:0;
      gimp_view_renderer_render_temp_buf(renderer,window,buffer,0,0,channel,GIMP_VIEW_BG_WHITE,GIMP_VIEW_BG_WHITE);
      cairo_surface_flush(renderer->surface);
      const guint32 *pixels=(const guint32*)cairo_image_surface_get_data(renderer->surface);
      for(int x=0;x<2;++x) {
        guint32 value=component?(x?200:32):(x?90:50);
        g_assert_cmphex(pixels[x],==,0xff000000u|value|(value<<8)|(value<<16));
      }
    }
    gimp_temp_buf_unref(buffer);g_object_unref(renderer);
  }
}
/* The layer renderer owns one canvas. Exercise its public event and draw paths,
 * rather than relying on the removed per-layer GtkListBox/GimpView children. */
static GtkWidget *layer_canvas(GtkWidget *owner) {
  GtkWidget *canvas=find(owner,"painter-layer-canvas");
  g_assert_true(GTK_IS_DRAWING_AREA(canvas));return canvas;
}
static gboolean layer_position(GList *items,GimpLayer *target,gint depth,gint *row,gint *out_depth) {
  for(GList *i=items;i;i=i->next) {
    GimpLayer *item=i->data;
    if(item==target){*out_depth=depth;return TRUE;}
    ++*row;
    GimpContainer *children=gimp_viewable_get_children(GIMP_VIEWABLE(item));
    if(children&&gimp_viewable_get_expanded(GIMP_VIEWABLE(item))&&
       layer_position(GIMP_LIST(children)->queue->head,target,depth+1,row,out_depth))return TRUE;
  }
  return FALSE;
}
static gboolean canvas_button(GtkWidget *canvas,GdkEventType type,guint button,
                              GdkModifierType state,gdouble x,gdouble y) {
  GdkEvent *event=gdk_event_new(type);
  event->button.window=g_object_ref(gtk_widget_get_window(canvas));
  event->button.send_event=TRUE;event->button.time=GDK_CURRENT_TIME;
  event->button.button=button;event->button.state=state;event->button.x=x;event->button.y=y;
  gdk_event_set_device(event,gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(canvas))));
  gboolean handled=gtk_widget_event(canvas,event);gdk_event_free(event);return handled;
}
static gboolean layer_button(GtkWidget *owner,GimpLayer *target,guint button,GdkModifierType state,gboolean eye) {
  GimpImage *image=gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(owner));
  gint row=0,depth=0;
  g_assert_true(layer_position(gimp_image_get_layer_iter(image),target,0,&row,&depth));
  GtkWidget *canvas=layer_canvas(owner);
  gboolean handled=canvas_button(canvas,GDK_BUTTON_PRESS,button,state,depth*8+(eye?8:28),row*64+(eye?8:32));
  if(button==1&&!gtk_widget_in_destruction(canvas))
    canvas_button(canvas,GDK_BUTTON_RELEASE,button,state,depth*8+(eye?8:28),row*64+(eye?8:32));
  return handled;
}
static cairo_surface_t *canvas_snapshot(GtkWidget *canvas) {
  /* GTK resize is frame-clock driven; settle it before offscreen drawing. */
  gtk_container_check_resize(GTK_CONTAINER(gtk_widget_get_toplevel(canvas)));
  GtkAllocation a;gtk_widget_get_allocation(canvas,&a);
  g_assert_cmpint(a.width,>,0);g_assert_cmpint(a.height,>,0);
  cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,a.width,a.height);
  cairo_t *cr=cairo_create(surface);gtk_widget_draw(canvas,cr);
  g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
  cairo_surface_flush(surface);return surface;
}
static guint32 canvas_pixel(cairo_surface_t *surface,gint x,gint y) {
  g_assert_cmpint(x,<,cairo_image_surface_get_width(surface));
  g_assert_cmpint(y,<,cairo_image_surface_get_height(surface));
  return *(guint32 *)(cairo_image_surface_get_data(surface)+y*cairo_image_surface_get_stride(surface)+x*4);
}
static void selection_and_visibility(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A"),*b=layer(image,NULL,"B");
  GtkWidget *w=tiles(image);g_assert_true(gimp_item_get_visible(GIMP_ITEM(b)));
  g_assert_true(layer_button(w,b,1,0,TRUE));drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(b)));g_assert_true(gimp_image_undo(image));drain();
  GList *sel=g_list_append(NULL,a);gimp_image_set_selected_layers(image,sel);g_list_free(sel);drain();
  cairo_surface_t *single=canvas_snapshot(layer_canvas(w));
  g_assert_true(layer_button(w,b,1,GDK_CONTROL_MASK,FALSE));drain();
  GList *selected=gimp_image_get_selected_layers(image);
  g_assert_cmpuint(g_list_length(selected),==,2);g_assert_nonnull(g_list_find(selected,a));g_assert_nonnull(g_list_find(selected,b));
  cairo_surface_t *multiple=canvas_snapshot(layer_canvas(w));
  /* Selection is painted in the gutter outside the thumbnail. */
  g_assert_cmphex(canvas_pixel(single,60,32),!=,canvas_pixel(multiple,60,32));
  g_assert_cmphex(canvas_pixel(single,60,96),==,canvas_pixel(multiple,60,96));
  cairo_surface_destroy(single);cairo_surface_destroy(multiple);
  close_tiles(w);g_object_unref(image);
}
static void group_move_and_undo(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A"),*group=gimp_group_layer_new(image);
  gimp_image_add_layer(image,group,NULL,0,FALSE);GimpLayer *b=layer(image,group,"B");
  GtkWidget *w=tiles(image);g_assert_true(gimp_painter_layer_tiles_move(GIMP_PAINTER_LAYER_TILES(w),a,group,TRUE,FALSE));
  g_assert_true(gimp_layer_get_parent(a)==group);g_assert_false(gimp_painter_layer_tiles_move(GIMP_PAINTER_LAYER_TILES(w),group,b,TRUE,FALSE));
  g_assert_true(gimp_image_undo(image));g_assert_null(gimp_layer_get_parent(a));
  close_tiles(w);g_object_unref(image);
}
static void image_switch_disconnect(void) {
  GimpImage *a=new_image(),*b=new_image();GimpLayer *la=layer(a,NULL,"Old"),*lb=layer(b,NULL,"New");
  GtkWidget *w=tiles(a);gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),b);drain();
  gimp_object_set_name(GIMP_OBJECT(la),"Changed old");gimp_item_set_visible(GIMP_ITEM(la),FALSE,FALSE);drain();
  g_assert_true(layer_button(w,lb,1,0,TRUE));drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(lb)));
  gimp_context_set_image(gimp_get_user_context(gimp),a);drain();
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w))==b);
  gimp_context_set_image(gimp_get_user_context(gimp),NULL);close_tiles(w);g_object_unref(a);g_object_unref(b);
}
static gboolean send_button(GtkWidget *w,GdkEventType type,guint button) {
  GdkEventButton ev={0};gboolean handled=FALSE;ev.type=type;ev.button=button;ev.x=8;ev.y=8;
  g_signal_emit_by_name(w,type==GDK_BUTTON_PRESS?"button-press-event":"button-release-event",&ev,&handled);return handled;
}
/* 08.016: custom Cairo canvas activation and lifecycle contracts. */
static void renderer_wait_viewable(GtkWidget *w) {
  gint64 deadline=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
  while((!gtk_widget_get_mapped(w)||!gdk_window_is_viewable(gtk_widget_get_window(w)))&&g_get_monotonic_time()<deadline) {
    g_main_context_iteration(NULL,FALSE);g_usleep(1000);
  }
  g_assert_true(gtk_widget_get_realized(w));g_assert_true(gtk_widget_get_mapped(w));
  g_assert_true(gdk_window_is_viewable(gtk_widget_get_window(w)));
}
static gboolean renderer_key_event(GtkWidget *w,guint keyval,GdkModifierType state) {
  renderer_wait_viewable(w);
  GdkEvent *event=gdk_event_new(GDK_KEY_PRESS);
  event->key.window=g_object_ref(gtk_widget_get_window(w));
  event->key.send_event=TRUE;event->key.time=GDK_CURRENT_TIME;event->key.keyval=keyval;event->key.state=state;
  gdk_event_set_device(event,gdk_seat_get_keyboard(gdk_display_get_default_seat(gtk_widget_get_display(w))));
  gboolean handled=gtk_widget_event(w,event);gdk_event_free(event);return handled;
}
static GtkWidget *renderer_current_popup(GtkWidget *owner) {
  GtkWidget *popup=gtk_grab_get_current();
  g_assert_true(GTK_IS_WINDOW(popup));
  g_assert_cmpint(gtk_window_get_window_type(GTK_WINDOW(popup)),==,GTK_WINDOW_POPUP);
  g_assert_true(gtk_widget_get_mapped(owner));
  g_assert_cmpstr(gtk_widget_get_name(popup),==,"painter-layer-properties-popup");
  g_assert_true(gtk_widget_get_mapped(popup));return popup;
}
static gboolean renderer_count_draw(GtkWidget *w,cairo_t *cr,gpointer data) {
  (void)w;(void)cr;(*(guint *)data)++;return FALSE;
}
static void renderer_native_size_draw(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"Native renderer");GtkWidget *w=tiles(image);
  GtkWidget *preview=layer_canvas(w),*button=find(w,"layer-popup");
  g_assert_nonnull(preview);g_assert_true(GTK_IS_BUTTON(button));
  GtkWidget *widgets[]={button,preview,w};
  for(guint direction=0;direction<2;direction++)for(guint state=0;state<2;state++) {
    gtk_widget_set_direction(w,direction?GTK_TEXT_DIR_RTL:GTK_TEXT_DIR_LTR);
    gtk_widget_set_sensitive(button,!state);drain();
    for(guint i=0;i<G_N_ELEMENTS(widgets);i++) {
      GtkWidget *widget=widgets[i];g_assert_nonnull(widget);
      gtk_widget_set_direction(widget,direction?GTK_TEXT_DIR_RTL:GTK_TEXT_DIR_LTR);drain();
      g_assert_cmpint(gtk_widget_get_direction(widget),==,direction?GTK_TEXT_DIR_RTL:GTK_TEXT_DIR_LTR);
      g_assert_true(gtk_widget_get_realized(widget));
      gint minimum,natural;gtk_widget_get_preferred_width(widget,&minimum,&natural);
      g_assert_cmpint(minimum,>,0);g_assert_cmpint(natural,>=,minimum);
      gtk_widget_get_preferred_height(widget,&minimum,&natural);
      g_assert_cmpint(minimum,>,0);g_assert_cmpint(natural,>=,minimum);
      GtkAllocation allocation;gtk_widget_get_allocation(widget,&allocation);
      g_assert_cmpint(allocation.width,>,0);g_assert_cmpint(allocation.height,>,0);
      gtk_widget_size_allocate(widget,&allocation);
      g_assert_true(gtk_widget_is_drawable(widget));
      cairo_surface_t *surface=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,allocation.width,allocation.height);
      cairo_t *cr=cairo_create(surface);guint draws=0;
      gulong handler=g_signal_connect(widget,"draw",G_CALLBACK(renderer_count_draw),&draws);
      gtk_widget_draw(widget,cr);g_signal_handler_disconnect(widget,handler);
      g_assert_cmpuint(draws,==,1);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);
      g_assert_cmpint(cairo_surface_status(surface),==,CAIRO_STATUS_SUCCESS);
      cairo_destroy(cr);cairo_surface_destroy(surface);
    }
  }
  close_tiles(w);g_object_unref(image);
}
static void renderer_native_activation_routes(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A"),*b=layer(image,NULL,"B");
  gimp_layer_set_opacity(a,.37,FALSE);gimp_layer_set_opacity(b,.81,FALSE);
  GtkWidget *w=tiles(image);g_assert_nonnull(layer_canvas(w));
  for(guint route=0;route<4;route++) {
    if(route==0)g_assert_true(layer_button(w,a,3,0,FALSE));
    else if(route==1)gtk_button_clicked(GTK_BUTTON(find(w,"layer-popup")));
    else g_assert_true(renderer_key_event(w,route==2?GDK_KEY_Menu:GDK_KEY_F10,route==2?0:GDK_SHIFT_MASK));
    drain();GtkWidget *popup=renderer_current_popup(w);
    GList *selection=gimp_image_get_selected_layers(image);g_assert_cmpuint(g_list_length(selection),==,1);g_assert_true(selection->data==a);
    GtkWidget *opacity=find(popup,"painter-layer-opacity");g_assert_nonnull(opacity);
    g_assert_cmpfloat_with_epsilon(gtk_range_get_value(GTK_RANGE(opacity)),37.,.000001);
    gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(w));drain();
    g_assert_null(gtk_grab_get_current());
  }
  close_tiles(w);g_object_unref(image);
}
typedef struct { GtkWidget *owner;GimpLayer *original,*newer;guint calls;gboolean fired,closed;guint late_focus; } RendererReentry;
static void renderer_select_newer(GimpImage *image,gpointer data) {
  RendererReentry *f=data;if(f->fired)return;f->fired=TRUE;f->calls++;
  GList *incoming=gimp_image_get_selected_layers(image);g_assert_cmpuint(g_list_length(incoming),==,1);g_assert_true(incoming->data==f->original);
  GList *selection=g_list_append(NULL,f->newer);gimp_image_set_selected_layers(image,selection);g_list_free(selection);
}
static void renderer_selection_reentry_case(gboolean superseded) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A"),*b=layer(image,NULL,"B");GtkWidget *w=tiles(image);
  GList *selection=g_list_append(NULL,b);gimp_image_set_selected_layers(image,selection);g_list_free(selection);drain();
  RendererReentry f={0};f.owner=w;f.original=a;f.newer=superseded?b:a;
  g_signal_connect(image,"selected-layers-changed",G_CALLBACK(renderer_select_newer),&f);
  g_assert_nonnull(layer_canvas(w));
  g_assert_true(layer_button(w,a,3,0,FALSE));drain();
  g_assert_cmpuint(f.calls,==,1);g_assert_cmpuint(g_list_length(gimp_image_get_selected_layers(image)),==,1);g_assert_true(gimp_image_get_selected_layers(image)->data==f.newer);
  /* A same-target notification completes the activation; B supersedes it. */
  if(superseded)g_assert_null(gtk_grab_get_current());
  else renderer_current_popup(w);
  g_signal_handlers_disconnect_by_data(image,&f);close_tiles(w);g_object_unref(image);
}
static void renderer_selection_reentry(void) { renderer_selection_reentry_case(TRUE); }
static void renderer_selection_same_target(void) { renderer_selection_reentry_case(FALSE); }
static void renderer_closed_destroy(GimpPainterLayerTiles *owner,gpointer data) {
  RendererReentry *f=data;gtk_widget_destroy(GTK_WIDGET(owner));f->closed=TRUE;
}
static gboolean renderer_focus_observe(GSignalInvocationHint *hint,guint count,const GValue *values,gpointer data) {
  RendererReentry *f=data;(void)hint;
  if(count && g_value_get_object(&values[0])==G_OBJECT(f->owner) && f->closed)f->late_focus++;
  return TRUE;
}
static void renderer_escape_owner_close(void) {
  GimpImage *image=new_image();layer(image,NULL,"A");GtkWidget *w=tiles(image);g_object_ref(w);
  g_assert_nonnull(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE));drain();
  RendererReentry f={0};f.owner=w;
  g_signal_connect(w,"popup-closed",G_CALLBACK(renderer_closed_destroy),&f);
  guint focus_signal=g_signal_lookup("grab-focus",GTK_TYPE_WIDGET);GSignalQuery query;
  g_signal_query(focus_signal,&query);g_assert_false(query.signal_flags&G_SIGNAL_NO_HOOKS);
  gulong focus_hook=g_signal_add_emission_hook(focus_signal,0,renderer_focus_observe,&f,NULL);
  g_assert_cmpuint(focus_hook,!=,0);
  g_assert_true(renderer_key_event(w,GDK_KEY_Escape,0));drain();
  g_signal_remove_emission_hook(focus_signal,focus_hook);
  g_assert_true(f.closed);g_assert_cmpuint(f.late_focus,==,0);
  g_assert_null(gtk_grab_get_current());
  g_signal_handlers_disconnect_by_data(w,&f);g_object_unref(w);g_object_unref(image);
}

static void renderer_stale_preview_disconnected(void) {
  GimpImage *a=new_image(),*b=new_image();GimpLayer *old=layer(a,NULL,"Old"),*newer=layer(b,NULL,"New");
  GtkWidget *w=tiles(a),*canvas=layer_canvas(w);g_object_ref(canvas);
  gpointer weak_canvas=canvas;g_object_add_weak_pointer(G_OBJECT(canvas),&weak_canvas);
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),b);drain();
  g_assert_true(layer_canvas(w)==canvas);
  cairo_surface_t *before=canvas_snapshot(canvas);
  gimp_item_set_visible(GIMP_ITEM(old),FALSE,FALSE);
  gimp_viewable_invalidate_preview(GIMP_VIEWABLE(old));drain();
  cairo_surface_t *after=canvas_snapshot(canvas);
  /* Compare the complete tile, including eye and selection gutter. Frame-clock
   * resizing can change the surrounding canvas allocation between snapshots;
   * that is independent of whether the old image still drives these pixels. */
  g_assert_cmpint(cairo_image_surface_get_width(before),>=,64);
  g_assert_cmpint(cairo_image_surface_get_height(before),>=,64);
  g_assert_cmpint(cairo_image_surface_get_width(after),>=,64);
  g_assert_cmpint(cairo_image_surface_get_height(after),>=,64);
  for(gint y=0;y<64;y++)
    g_assert_cmpmem(cairo_image_surface_get_data(before)+y*cairo_image_surface_get_stride(before),64*4,
                   cairo_image_surface_get_data(after)+y*cairo_image_surface_get_stride(after),64*4);
  g_assert_true(layer_button(w,newer,1,0,TRUE));g_assert_false(gimp_item_get_visible(GIMP_ITEM(newer)));
  g_assert_true(gimp_image_get_selected_layers(b)->data==newer);
  cairo_surface_destroy(before);cairo_surface_destroy(after);
  close_tiles(w);
  /* The retained, destroyed canvas must have disconnected all callbacks. */
  g_assert_false(send_button(canvas,GDK_BUTTON_PRESS,3));
  g_assert_false(send_button(canvas,GDK_BUTTON_RELEASE,1));
  g_object_unref(canvas);drain();g_assert_null(weak_canvas);
  g_object_unref(a);g_object_unref(b);
}

static void renderer_modifier_activation(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A"),*b=layer(image,NULL,"B");GtkWidget *w=tiles(image);
  g_assert_nonnull(layer_canvas(w));
  GdkModifierType states[]={0,GDK_CONTROL_MASK,GDK_SHIFT_MASK,GDK_CONTROL_MASK,GDK_CONTROL_MASK};
  for(guint i=0;i<G_N_ELEMENTS(states);i++) {
    GList *selection=g_list_append(NULL,i==3?a:b);
    if(i==0||i==4)selection=g_list_append(selection,a);
    gimp_image_set_selected_layers(image,selection);g_list_free(selection);drain();
    g_assert_true(layer_button(w,a,3,states[i],FALSE));drain();
    renderer_current_popup(w);selection=gimp_image_get_selected_layers(image);
    g_assert_cmpuint(g_list_length(selection),==,i>=3?1:2);
    if(i!=4)
      g_assert_nonnull(g_list_find(selection,a));
    if(i!=3)
      g_assert_nonnull(g_list_find(selection,b));
    gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(w));drain();
  }
  close_tiles(w);g_object_unref(image);
}

static void long_press_cancel_destroy(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image),*add=find(w,"painter-layer-add");
  g_object_ref(w);g_object_ref(add);
  send_button(add,GDK_BUTTON_PRESS,1);g_assert_true(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  GdkEventMotion ev={0};gboolean handled;ev.type=GDK_MOTION_NOTIFY;ev.x=60;ev.y=8;g_signal_emit_by_name(add,"motion-notify-event",&ev,&handled);
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  send_button(add,GDK_BUTTON_PRESS,1);close_tiles(w);g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  send_button(add,GDK_BUTTON_RELEASE,1);g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  g_object_unref(add);g_object_unref(w);g_object_unref(image);drain();
}
static void preview_pending_destroy(void) {
  GimpImage *image=new_image();GimpLayer *l=layer(image,NULL,"Preview");GtkWidget *w=tiles(image);
  for(int i=0;i<10;++i) gimp_viewable_invalidate_preview(GIMP_VIEWABLE(l));
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),NULL);close_tiles(w);g_object_unref(image);drain();
}
static void popup_repeat_and_owner_close(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image);
  for(int i=0;i<5;++i) { GtkWidget *p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);g_assert_nonnull(p);g_signal_emit_by_name(p,"cancel");drain(); }
  GtkWidget *p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),TRUE);g_object_ref(p);
  GtkWidget *button=find(p,"layers-new-clone");g_object_ref(button);close_tiles(w);
  gtk_button_clicked(GTK_BUTTON(button));g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  g_object_unref(button);g_object_unref(p);g_object_unref(image);drain();
}
static void count_popup_closed(GimpPainterLayerTiles *w,gpointer data) {
  (void)w;(*(guint *)data)++;
}
static void native_popup_wait_viewable(GtkWidget *popup) {
  gint64 deadline=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
  while(!gdk_window_is_viewable(gtk_widget_get_window(popup)) && g_get_monotonic_time()<deadline) {
    g_main_context_iteration(NULL,FALSE);g_usleep(1000);
  }
  g_assert_true(gdk_window_is_viewable(gtk_widget_get_window(popup)));
}
static gboolean native_popup_key(GtkWidget *popup,guint keyval) {
  native_popup_wait_viewable(popup);
  GdkEvent *event=gdk_event_new(GDK_KEY_PRESS);
  event->key.window=g_object_ref(gtk_widget_get_window(popup));
  event->key.send_event=TRUE;event->key.time=GDK_CURRENT_TIME;event->key.keyval=keyval;
  gdk_event_set_device(event,gdk_seat_get_keyboard(gdk_display_get_default_seat(gtk_widget_get_display(popup))));
  gboolean handled=gtk_widget_event(popup,event);gdk_event_free(event);return handled;
}
static void native_popup_button(GtkWidget *popup,GdkEventType type,gdouble x,gdouble y) {
  if(type==GDK_BUTTON_PRESS)native_popup_wait_viewable(popup);
  GdkEvent *event=gdk_event_new(type);
  event->button.window=g_object_ref(gtk_widget_get_window(popup));
  event->button.send_event=TRUE;event->button.time=GDK_CURRENT_TIME;
  event->button.button=1;event->button.x=x;event->button.y=y;
  gdk_event_set_device(event,gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(popup))));
  gtk_widget_event(popup,event);gdk_event_free(event);
}
static void popup_native_events_and_parent(void) {
  GimpImage *image=new_image();GimpLayer *selected=layer(image,NULL,"Layer");GtkWidget *w=tiles(image);
  GtkSettings *settings=gtk_widget_get_settings(w);gboolean animations=FALSE;
  g_object_get(settings,"gtk-enable-animations",&animations,NULL);
  g_object_set(settings,"gtk-enable-animations",FALSE,NULL);
  guint closed=0;g_signal_connect(w,"popup-closed",G_CALLBACK(count_popup_closed),&closed);
  for(guint cycle=0;cycle<3;cycle++) {
    GtkWidget *popup=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);g_assert_nonnull(popup);
    g_assert_true(GTK_IS_WINDOW(popup));
    g_assert_cmpint(gtk_window_get_window_type(GTK_WINDOW(popup)),==,GTK_WINDOW_POPUP);
    /* Native windows destroy on Escape; retain the old host for assertions. */
    g_object_ref(popup);
    GtkWidget *frame=gtk_bin_get_child(GTK_BIN(popup));g_assert_true(GTK_IS_FRAME(frame));
    GtkWidget *scroll=gtk_bin_get_child(GTK_BIN(frame));g_assert_true(GTK_IS_SCROLLED_WINDOW(scroll));
    GtkWidget *viewport=gtk_bin_get_child(GTK_BIN(scroll));g_assert_true(GTK_IS_VIEWPORT(viewport));
    g_assert_true(GTK_IS_BOX(gtk_bin_get_child(GTK_BIN(viewport))));drain();
    g_assert_true(gtk_widget_get_realized(popup));g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
    GtkWidget *opacity=find(popup,"painter-layer-opacity");g_assert_nonnull(opacity);
    gtk_widget_grab_focus(opacity);
    gtk_range_set_value(GTK_RANGE(opacity),70.);
    g_assert_cmpfloat(gimp_layer_get_opacity(selected),==,.7);
    /* Exercise the native inherited event handler, not a synthetic close signal. */
    g_assert_true(native_popup_key(popup,GDK_KEY_Escape));drain();
    g_assert_false(gtk_widget_get_visible(popup));g_assert_false(gtk_widget_has_grab(popup));
    g_assert_cmpuint(closed,==,cycle*2+1);g_object_unref(popup);
    popup=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),TRUE);g_assert_nonnull(popup);drain();
    g_assert_true(gtk_popover_get_relative_to(GTK_POPOVER(popup))==find(w,"painter-layer-add"));
    g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
    native_popup_wait_viewable(popup);
    GtkAllocation allocation;gtk_widget_get_allocation(gtk_bin_get_child(GTK_BIN(popup)),&allocation);
    g_assert_cmpint(allocation.width,>,0);g_assert_cmpint(allocation.height,>,0);
    gdouble x=allocation.x+allocation.width/2.,y=allocation.y+allocation.height/2.;
    native_popup_button(popup,GDK_BUTTON_PRESS,x,y);native_popup_button(popup,GDK_BUTTON_RELEASE,x,y);drain();
    g_assert_true(gtk_widget_get_visible(popup));g_assert_cmpuint(closed,==,cycle*2+1);
    native_popup_button(popup,GDK_BUTTON_PRESS,-10,-10);native_popup_button(popup,GDK_BUTTON_RELEASE,-10,-10);drain();
    g_assert_false(gtk_widget_get_visible(popup));g_assert_false(gtk_widget_has_grab(popup));
    g_assert_cmpuint(closed,==,cycle*2+2);g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  }
  g_signal_handlers_disconnect_by_data(w,&closed);
  gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(w));
  g_object_set(settings,"gtk-enable-animations",animations,NULL);
  close_tiles(w);g_object_unref(image);
}
static void popup_destroy_orders(void) {
  for(guint owner_first=0;owner_first<2;owner_first++) {
    GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image);g_object_ref(w);
    GtkWidget *popup=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),TRUE);g_assert_nonnull(popup);g_object_ref(popup);
    GtkWidget *button=find(popup,"layers-new-clone");g_assert_nonnull(button);g_object_ref(button);
    drain();g_assert_true(gtk_widget_get_realized(popup));g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
    GtkWidget *next=NULL;gpointer weak_next=NULL;
    gpointer weak_owner=w,weak_popup=popup,weak_button=button;
    g_object_add_weak_pointer(G_OBJECT(w),&weak_owner);g_object_add_weak_pointer(G_OBJECT(popup),&weak_popup);
    g_object_add_weak_pointer(G_OBJECT(button),&weak_button);
    if(owner_first)gtk_widget_destroy(w);
    else {
      gtk_widget_destroy(popup);drain();g_assert_false(gtk_widget_has_grab(popup));
      next=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);g_assert_nonnull(next);g_object_ref(next);
      weak_next=next;g_object_add_weak_pointer(G_OBJECT(next),&weak_next);drain();
      g_assert_true(gtk_widget_get_mapped(next));g_assert_true(gtk_widget_has_grab(next));gtk_widget_destroy(w);
    }
    gtk_widget_destroy(w);gtk_button_clicked(GTK_BUTTON(button));
    g_assert_false(gtk_widget_has_grab(popup));g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
    if(next){g_assert_false(gtk_widget_has_grab(next));g_object_unref(next);}
    g_object_unref(button);g_object_unref(popup);g_object_unref(w);drain();
    g_assert_null(weak_button);g_assert_null(weak_popup);g_assert_null(weak_owner);g_assert_null(weak_next);g_object_unref(image);
  }
}
typedef struct { GtkWidget *newer; guint calls; gboolean creation; } PopupReplacement;
static void replace_on_popup_closed(GimpPainterLayerTiles *w,gpointer data) {
  PopupReplacement *f=data;
  if(f->calls++==0) {
    f->newer=gimp_painter_layer_tiles_popup(w,f->creation);
    g_assert_nonnull(f->newer);g_object_ref(f->newer);
  }
}
static void popup_replacement_newer_same_image(gboolean creation) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image);
  GtkWidget *first=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);
  g_assert_nonnull(first);g_object_ref(first);
  GtkWidget *stale=find(first,"layers-new-clone");g_assert_nonnull(stale);g_object_ref(stale);
  PopupReplacement f={NULL,0,!creation};
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(replace_on_popup_closed),&f);
  g_assert_null(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),creation));
  g_assert_cmpuint(f.calls,==,1);g_assert_nonnull(f.newer);
  g_assert_cmpstr(gtk_widget_get_name(f.newer),==,f.creation?"painter-layer-create-popup":"painter-layer-properties-popup");
  g_assert_true(gtk_widget_get_visible(f.newer));g_assert_false(gtk_widget_get_visible(first));
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w))==image);
  /* An old action cannot run after its popup was retired. */
  gtk_button_clicked(GTK_BUTTON(stale));g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  gpointer weak_first=first,weak_newer=f.newer;
  g_object_add_weak_pointer(G_OBJECT(first),&weak_first);
  g_object_add_weak_pointer(G_OBJECT(f.newer),&weak_newer);
  g_signal_handler_disconnect(w,handler);
  gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(w));
  gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(w));
  g_assert_false(gtk_widget_get_visible(f.newer));
  g_assert_false(gtk_widget_has_grab(first));g_assert_false(gtk_widget_has_grab(f.newer));
  g_object_unref(stale);g_object_unref(first);g_object_unref(f.newer);drain();
  g_assert_null(weak_first);g_assert_null(weak_newer);
  close_tiles(w);g_object_unref(image);
}
static void popup_properties_newer_same_image(void) { popup_replacement_newer_same_image(FALSE); }
static void popup_creation_newer_same_image(void) { popup_replacement_newer_same_image(TRUE); }
static void destroy_on_popup_closed(GimpPainterLayerTiles *w,gpointer data) {
  guint *calls=data;
  ++*calls;
  gtk_widget_destroy(GTK_WIDGET(w));
}
static void popup_replacement_owner_close(gboolean creation) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image);
  gpointer weak=w;
  guint calls=0;
  g_object_ref(w);
  g_object_add_weak_pointer(G_OBJECT(w),&weak);
  g_assert_nonnull(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),creation));
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(destroy_on_popup_closed),&calls);
  /* Closing the first popup destroys its owner before the replacement resumes. */
  g_assert_null(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),creation));
  g_assert_cmpuint(calls,==,1);
  g_assert_null(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w)));
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  /* GtkWidget destruction may already have disconnected this handler. */
  if (g_signal_handler_is_connected(w,handler))
    g_signal_handler_disconnect(w,handler);
  g_object_unref(w);drain();g_assert_null(weak);
  g_object_unref(image);
}
static void popup_properties_reentrant_owner_close(void) { popup_replacement_owner_close(FALSE); }
static void popup_creation_reentrant_owner_close(void) { popup_replacement_owner_close(TRUE); }
typedef struct { GimpImage *image; guint calls; } PopupImageSwitch;
static void switch_on_popup_closed(GimpPainterLayerTiles *w,gpointer data) {
  PopupImageSwitch *switch_image=data;
  ++switch_image->calls;
  gimp_painter_layer_tiles_set_image(w,switch_image->image);
}
static void popup_replacement_image_switch(gboolean creation) {
  GimpImage *a=new_image(),*b=new_image();layer(a,NULL,"Old");layer(b,NULL,"New");
  GtkWidget *w=tiles(a);
  PopupImageSwitch switch_image={b,0};
  g_assert_nonnull(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),creation));
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(switch_on_popup_closed),&switch_image);
  g_assert_null(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),creation));
  g_assert_cmpuint(switch_image.calls,==,1);
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w))==b);
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  g_assert_cmpint(gimp_image_get_n_layers(a),==,1);
  g_assert_cmpint(gimp_image_get_n_layers(b),==,1);
  g_signal_handler_disconnect(w,handler);
  g_assert_nonnull(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),creation));
  close_tiles(w);g_object_unref(a);g_object_unref(b);
}
static void popup_properties_reentrant_image_switch(void) { popup_replacement_image_switch(FALSE); }
static void popup_creation_reentrant_image_switch(void) { popup_replacement_image_switch(TRUE); }
/* These clicks enter the live action before popup-closed reenters. They are
 * distinct from emitting on an already disconnected, retired button. */
static GtkWidget *new_group_action(GtkWidget *w) {
  GtkWidget *p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),TRUE);
  g_assert_nonnull(p);
  GtkWidget *button=find(p,"layers-new-group");g_assert_nonnull(button);
  return g_object_ref(button);
}
static void popup_action_image_switch(void) {
  GimpImage *a=new_image(),*b=new_image();layer(a,NULL,"Old");layer(b,NULL,"New");
  GtkWidget *w=tiles(a),*button=new_group_action(w);
  PopupImageSwitch f={b,0};
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(switch_on_popup_closed),&f);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(f.calls,==,1);
  g_assert_true(action_data_get_image(w)==b);
  g_test_message("A layers=%d; B layers=%d",gimp_image_get_n_layers(a),gimp_image_get_n_layers(b));
  g_assert_cmpint(gimp_image_get_n_layers(a),==,1);
  g_assert_cmpint(gimp_image_get_n_layers(b),==,1);
  g_signal_handler_disconnect(w,handler);g_object_unref(button);
  close_tiles(w);g_object_unref(a);g_object_unref(b);
}
static void popup_action_newer_same_image(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");
  GtkWidget *w=tiles(image),*button=new_group_action(w);
  PopupReplacement f={NULL,0,FALSE};
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(replace_on_popup_closed),&f);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(f.calls,==,1);g_assert_nonnull(f.newer);
  g_assert_true(action_data_get_image(w)==image);
  g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  g_assert_true(gtk_widget_get_visible(f.newer));
  g_signal_handler_disconnect(w,handler);g_object_unref(button);
  close_tiles(w);g_object_unref(f.newer);g_object_unref(image);
}
typedef struct { GimpContext *context; guint calls; } PopupContextSwitch;
static void context_on_popup_closed(GimpPainterLayerTiles *w,gpointer data) {
  PopupContextSwitch *f=data;++f->calls;
  g_object_set(w,"context",f->context,NULL);
}
static void popup_action_context_switch(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");
  GtkWidget *w=tiles(image),*button=new_group_action(w);
  GimpContext *old=g_object_ref(action_data_get_context(w));
  GimpContext *context=gimp_context_new(gimp,"action-replacement",gimp_get_user_context(gimp));
  PopupContextSwitch f={context,0};
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(context_on_popup_closed),&f);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(f.calls,==,1);
  g_assert_true(action_data_get_image(w)==image);
  g_assert_nonnull(action_data_get_context(w));
  g_assert_true(action_data_get_context(w)!=old);
  g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  g_signal_handler_disconnect(w,handler);g_object_unref(button);
  close_tiles(w);g_object_unref(old);g_object_unref(context);g_object_unref(image);
}
static void popup_action_owner_close(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");
  GtkWidget *w=tiles(image),*button=new_group_action(w);gpointer weak=w;
  guint calls=0;g_object_add_weak_pointer(G_OBJECT(w),&weak);
  g_signal_connect(w,"popup-closed",G_CALLBACK(destroy_on_popup_closed),&calls);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(calls,==,1);g_object_unref(button);drain();
  /* Require finalization, not merely a null image making a late command inert. */
  g_assert_null(weak);g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  g_object_unref(image);
}
typedef struct { GimpImage *image; guint calls, updates; gboolean armed; } MenuActionSwitch;
static void menu_switch_on_popup_closed(GimpPainterLayerTiles *w,gpointer data) {
  MenuActionSwitch *f=data;++f->calls;
  gimp_painter_layer_tiles_set_image(w,f->image);
  f->armed=TRUE;
}
static void rejected_menu_update(GimpUIManager *manager,gpointer update_data,gpointer data) {
  MenuActionSwitch *f=data;(void)manager;(void)update_data;
  if (!f->armed) return;
  ++f->updates;
  /* Reaching native menu dispatch is already the regression. Stop before
   * opening a context menu without a hardware event in this fixture. */
  g_assert_cmpuint(f->updates,==,0);
}
static void popup_action_layer_menu_switch(void) {
  GimpImage *a=new_image(),*b=new_image();layer(a,NULL,"Old");layer(b,NULL,"New");
  GtkWidget *w=tiles(a),*p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);
  GtkWidget *button=find(p,"layer-menu");g_assert_nonnull(button);g_object_ref(button);
  MenuActionSwitch f={b,0,0,FALSE};
  GimpUIManager *manager=gimp_editor_get_ui_manager(GIMP_EDITOR(w));g_object_ref(manager);
  gulong update_handler=g_signal_connect(manager,"update",G_CALLBACK(rejected_menu_update),&f);
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(menu_switch_on_popup_closed),&f);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(f.calls,==,1);g_assert_true(action_data_get_image(w)==b);
  g_assert_cmpuint(f.updates,==,0);
  g_signal_handler_disconnect(manager,update_handler);g_signal_handler_disconnect(w,handler);
  g_object_unref(manager);g_object_unref(button);close_tiles(w);g_object_unref(a);g_object_unref(b);
}
static void popup_action_normal_group(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");
  GtkWidget *w=tiles(image),*button=new_group_action(w);
  gtk_button_clicked(GTK_BUTTON(button));g_object_unref(button);
  g_assert_cmpint(gimp_image_get_n_layers(image),==,2);
  g_assert_true(GIMP_IS_GROUP_LAYER(gimp_image_get_selected_layers(image)->data));
  g_assert_true(gimp_image_undo(image));g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  close_tiles(w);g_object_unref(image);
}
/* The C entry and inherited image-editor entry must share the same terminal
 * lifecycle. Holding a reference after GtkWidget::destroy is legal. */
static void destroyed_tiles_reject_image(void) {
  GimpImage *a=new_image(),*b=new_image();layer(a,NULL,"Old");layer(b,NULL,"New");
  GtkWidget *w=tiles(a);gpointer weak=w;
  g_object_ref(w);g_object_add_weak_pointer(G_OBJECT(w),&weak);
  g_assert_true(action_data_get_gimp(w)==gimp);
  g_assert_true(action_data_get_context(w)==GIMP_IMAGE_EDITOR(w)->context);
  g_assert_true(action_data_get_image(w)==a);
  g_assert_null(action_data_get_display(w));
  close_tiles(w);
  g_assert_null(action_data_get_context(w));
  g_assert_null(action_data_get_image(w));
  g_assert_null(action_data_get_display(w));
  g_assert_true(g_type_is_a(G_OBJECT_TYPE(w),GIMP_TYPE_IMAGE_EDITOR));
  g_assert_true(GTK_IS_BOX(w));
  g_assert_null(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w)));
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),b);
  g_assert_null(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w)));
  gimp_image_editor_set_image(GIMP_IMAGE_EDITOR(w),a);
  g_assert_null(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w)));
  g_assert_null(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),TRUE));
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  gtk_widget_destroy(w);g_object_unref(w);drain();g_assert_null(weak);
  /* If the parent reconnected, this would invoke a callback on the freed owner. */
  gimp_image_flush(a);gimp_image_flush(b);drain();
  g_object_unref(a);g_object_unref(b);
}
static void image_switch_popup_owner_close(void) {
  GimpImage *a=new_image(),*b=new_image();layer(a,NULL,"Old");layer(b,NULL,"New");
  GtkWidget *w=tiles(a);gpointer weak=w;guint calls=0;
  g_object_ref(w);g_object_add_weak_pointer(G_OBJECT(w),&weak);
  g_assert_nonnull(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE));
  g_signal_connect(w,"popup-closed",G_CALLBACK(destroy_on_popup_closed),&calls);
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),b);
  g_assert_cmpuint(calls,==,1);
  g_assert_null(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w)));
  g_assert_null(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),TRUE));
  /* The interrupted bind must not retain its incoming image until finalization. */
  gpointer weak_image=b;g_object_add_weak_pointer(G_OBJECT(b),&weak_image);
  gimp_image_flush(b);drain();g_object_unref(b);drain();g_assert_null(weak_image);
  g_object_unref(w);drain();g_assert_null(weak);
  gimp_image_flush(a);drain();g_object_unref(a);
}
static void image_switch_popup_newer_image(void) {
  GimpImage *a=new_image(),*b=new_image(),*c=new_image();
  layer(a,NULL,"Original");GimpLayer *lb=layer(b,NULL,"Obsolete"),*lc=layer(c,NULL,"Latest");
  GtkWidget *w=tiles(a);PopupImageSwitch switch_image={c,0};
  g_assert_nonnull(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE));
  gulong handler=g_signal_connect(w,"popup-closed",G_CALLBACK(switch_on_popup_closed),&switch_image);
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),b);drain();
  g_assert_cmpuint(switch_image.calls,==,1);
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w))==c);
  g_assert_true(layer_button(w,lc,1,0,TRUE));drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(lc)));
  g_assert_true(gimp_item_get_visible(GIMP_ITEM(lb)));
  g_signal_handler_disconnect(w,handler);close_tiles(w);
  g_object_unref(a);g_object_unref(b);g_object_unref(c);
}
static void destroy_on_sensitive(GObject *object,GParamSpec *pspec,gpointer data) {
  guint *calls=data;(void)pspec;
  ++*calls;gtk_widget_destroy(GTK_WIDGET(object));
}
static void parent_image_notify_owner_close(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(NULL);
  gpointer weak=w;guint calls=0;g_object_ref(w);g_object_add_weak_pointer(G_OBJECT(w),&weak);
  g_signal_connect(w,"notify::sensitive",G_CALLBACK(destroy_on_sensitive),&calls);
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),image);
  g_assert_cmpuint(calls,>=,1);
  g_assert_null(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w)));
  g_assert_null(gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE));
  g_object_unref(w);drain();g_assert_null(weak);
  gimp_image_flush(image);drain();g_object_unref(image);
}
static void popup_controls_and_clone(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A");GtkWidget *w=tiles(image);
  GtkWidget *p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);
  GtkWidget *mode=find(p,"painter-layer-mode");g_assert_nonnull(mode);
  gimp_layer_mode_box_set_mode(GIMP_LAYER_MODE_BOX(mode),GIMP_LAYER_MODE_PAINTER_MULTIPLY);
  g_assert_cmpint(gimp_layer_get_mode(a),==,GIMP_LAYER_MODE_PAINTER_MULTIPLY);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(find(p,"painter-layer-lock-alpha")),TRUE);
  g_assert_true(gimp_layer_get_lock_alpha(a));
  gtk_range_set_value(GTK_RANGE(find(p,"painter-layer-opacity")),37);
  g_assert_cmpfloat(gimp_layer_get_opacity(a),==,.37);
  g_assert_nonnull(gimp_editor_get_ui_manager(GIMP_EDITOR(w)));
  gtk_button_clicked(GTK_BUTTON(find(p,"layers-new-clone")));drain();
  g_assert_cmpint(gimp_image_get_n_layers(image),==,2);
  GimpLayer *clone=gimp_image_get_selected_layers(image)->data;
  g_assert_true(GIMP_IS_CLONE_LAYER(clone));
  g_assert_true(gimp_clone_layer_get_source(GIMP_CLONE_LAYER(clone))==a);
  p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);
  g_assert_nonnull(find(p,"layers-edit-clone"));
  close_tiles(w);g_object_unref(image);
}
static void mask_controls(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A");
  GimpLayerMask *mask=gimp_layer_create_mask(a,GIMP_ADD_MASK_WHITE,NULL);GError *error=NULL;
  g_assert_nonnull(gimp_layer_add_mask(a,mask,FALSE,&error));g_assert_no_error(error);
  GtkWidget *w=tiles(image),*p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(find(p,"painter-layer-mask-edit")),TRUE);
  g_assert_true(gimp_layer_get_edit_mask(a));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(find(p,"painter-layer-mask-show")),TRUE);
  g_assert_true(gimp_layer_get_show_mask(a));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(find(p,"painter-layer-mask-apply")),FALSE);
  g_assert_false(gimp_layer_get_apply_mask(a));
  close_tiles(w);g_object_unref(image);
}
static void long_press_once(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image),*add=find(w,"painter-layer-add");
  send_button(add,GDK_BUTTON_PRESS,1);
  gint64 deadline=g_get_monotonic_time()+650000;
  while(g_get_monotonic_time()<deadline){g_main_context_iteration(NULL,FALSE);g_usleep(1000);}
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  send_button(add,GDK_BUTTON_RELEASE,1);g_assert_cmpint(gimp_image_get_n_layers(image),==,1);
  close_tiles(w);g_object_unref(image);
}
static void destroy_on_visible(GimpItem *item,gpointer data) { gtk_widget_destroy(GTK_WIDGET(data)); }
static void reentrant_visibility_close(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"Layer");GtkWidget *w=tiles(image);g_object_ref(w);
  g_signal_connect(a,"visibility-changed",G_CALLBACK(destroy_on_visible),w);
  GtkWidget *control=layer_canvas(w);g_object_ref(control);
  /* Lease the event target across synchronous destruction by visibility-changed. */
  g_assert_true(canvas_button(control,GDK_BUTTON_PRESS,1,0,8,8));
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(a)));drain();
  g_signal_handlers_disconnect_by_data(a,w);g_object_unref(control);
  g_object_unref(w);g_object_unref(image);
}
static void multiple_reorder_one_undo(void) {
  GimpImage *image=new_image();GimpLayer *d=layer(image,NULL,"D"),*c=layer(image,NULL,"C"),*b=layer(image,NULL,"B"),*a=layer(image,NULL,"A");
  GtkWidget *w=tiles(image);GList *selection=g_list_append(g_list_append(NULL,a),b);
  gimp_image_set_selected_layers(image,selection);g_list_free(selection);
  g_assert_true(gimp_painter_layer_tiles_move(GIMP_PAINTER_LAYER_TILES(w),a,c,FALSE,TRUE));
  GList *items=gimp_image_get_layer_iter(image);
  g_assert_true(items->data==c);g_assert_true(items->next->data==a);g_assert_true(items->next->next->data==b);g_assert_true(items->next->next->next->data==d);
  g_assert_cmpuint(g_list_length(gimp_image_get_selected_layers(image)),==,2);
  g_assert_true(gimp_image_undo(image));items=gimp_image_get_layer_iter(image);
  g_assert_true(items->data==a);g_assert_true(items->next->data==b);g_assert_true(items->next->next->data==c);
  close_tiles(w);g_object_unref(image);
}
static void hide_cancels_grab(void) {
  GimpImage *image=new_image();layer(image,NULL,"Layer");GtkWidget *w=tiles(image),*add=find(w,"painter-layer-add");
  send_button(add,GDK_BUTTON_PRESS,1);g_assert_true(gtk_widget_has_grab(add));
  gtk_widget_hide(w);drain();g_assert_false(gtk_widget_has_grab(add));
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  close_tiles(w);g_object_unref(image);
}
static GimpDisplayShell *new_shell(void) {
  gimp_set_focused_once(gimp);gimp_test_utils_create_image(gimp,256,256);drain();
  GimpDisplay *display=gimp_context_get_display(gimp_get_user_context(gimp));
  g_assert_nonnull(display);return gimp_display_get_shell(display);
}
static void close_shell(GimpDisplayShell *shell) {
  GimpImage *image=gimp_display_get_image(shell->display);
  gimp_display_delete(shell->display);g_object_unref(image);drain();
}
static GimpOverlayChild *layer_panel(GimpDisplayShell *shell) {
  GtkWidget *tiles=find(shell->canvas,"painter-layer-tiles");g_assert_nonnull(tiles);
  GtkWidget *frame=gtk_widget_get_parent(tiles);
  GimpOverlayChild *child=gimp_overlay_child_find(GIMP_OVERLAY_BOX(shell->canvas),frame);g_assert_nonnull(child);return child;
}
static GdkWindow *overlay_pick_window(GtkWidget *box,gdouble x,gdouble y) {
  GdkWindow *picked=NULL;
  g_signal_emit_by_name(gtk_widget_get_window(box),"pick-embedded-child",x,y,&picked);
  return picked;
}
static void overlay_topmost_visible_input(void) {
  GtkWidget *box=gimp_overlay_box_new();
  GtkWidget *lower=gtk_event_box_new(),*upper=gtk_event_box_new();
  gtk_widget_set_size_request(lower,100,100);gtk_widget_set_size_request(upper,100,100);
  gtk_container_add(GTK_CONTAINER(window),box);
  gimp_overlay_box_add_child(GIMP_OVERLAY_BOX(box),lower,0.,0.);
  gimp_overlay_box_add_child(GIMP_OVERLAY_BOX(box),upper,0.,0.);
  gimp_overlay_box_set_child_position(GIMP_OVERLAY_BOX(box),lower,10,10);
  gimp_overlay_box_set_child_position(GIMP_OVERLAY_BOX(box),upper,60,60);
  gtk_widget_show_all(window);gimp_test_run_temp_mainloop(100);drain();
  GimpOverlayChild *bottom=gimp_overlay_child_find(GIMP_OVERLAY_BOX(box),lower);
  GimpOverlayChild *top=gimp_overlay_child_find(GIMP_OVERLAY_BOX(box),upper);
  g_assert_cmpfloat(bottom->matrix.x0,==,10);g_assert_cmpfloat(top->matrix.x0,==,60);
  /* Later children paint last. Their visibly exposed midpoint must receive
   * input; the first child still owns its unobscured region. */
  g_assert_true(overlay_pick_window(box,80,80)==top->window);
  g_assert_true(overlay_pick_window(box,20,20)==bottom->window);
  g_assert_true(overlay_pick_window(box,140,140)==top->window);
  /* Insensitivity disables native actions, not occlusion. It must not make
   * a visible disabled control click through into the underlying panel. */
  gtk_widget_set_sensitive(upper,FALSE);
  g_assert_true(overlay_pick_window(box,80,80)==top->window);
  gtk_widget_hide(upper);drain();
  g_assert_true(overlay_pick_window(box,80,80)==bottom->window);
  g_assert_null(overlay_pick_window(box,140,140));
  gtk_widget_show(upper);drain();
  gimp_overlay_box_set_child_input_pass_through(GIMP_OVERLAY_BOX(box),upper,TRUE);
  g_assert_true(overlay_pick_window(box,80,80)==bottom->window);
  gimp_overlay_box_set_child_input_pass_through(GIMP_OVERLAY_BOX(box),upper,FALSE);
  g_assert_true(overlay_pick_window(box,80,80)==top->window);
  close_tiles(box);
}
static void overlay_input_and_restore(void) {
  gboolean expected_visible;
  g_object_get(gimp->config,"painter-canvas-ui",&expected_visible,NULL);
  GimpDisplayShell *shell=new_shell();
  /* Startup follows the current preference. Establish the hidden precondition
   * explicitly instead of assuming the historical FALSE default. */
  g_assert_cmpint(gimp_painter_canvas_ui_get_visible(shell),==,expected_visible);
  /* Check the shell-local hide synchronously: the native window may reapply
   * the unchanged global preference during subsequent event processing. */
  gimp_painter_canvas_ui_set_visible(shell,FALSE);
  g_assert_false(gimp_painter_canvas_ui_get_visible(shell));
  gimp_painter_canvas_ui_set_visible(shell,TRUE);drain();
  GimpOverlayChild *child=layer_panel(shell);
  g_assert_false(child->input_pass_through);g_assert_cmpfloat(child->opacity,==,.85);
  gimp_painter_canvas_ui_pointer(shell,child->x+10,child->y+10,FALSE);
  g_assert_true(child->input_pass_through);g_assert_cmpfloat(child->opacity,==,.1);
  g_assert_false(gimp_overlay_child_pick(GIMP_OVERLAY_BOX(shell->canvas),child,child->x+10,child->y+10));
  gimp_painter_canvas_ui_pointer(shell,0,0,TRUE);
  g_assert_false(child->input_pass_through);g_assert_cmpfloat(child->opacity,==,.85);
  /* Setting visual opacity alone never changes picking. */
  gimp_overlay_box_set_child_opacity(GIMP_OVERLAY_BOX(shell->canvas),child->widget,.1);
  g_assert_false(gimp_overlay_box_get_child_input_pass_through(GIMP_OVERLAY_BOX(shell->canvas),child->widget));
  for(int i=0;i<5;++i){gimp_painter_canvas_ui_set_visible(shell,FALSE);gimp_painter_canvas_ui_set_visible(shell,TRUE);}
  close_shell(shell);
}
static void dock_restore_and_parent_close(void) {
  GimpDisplayShell *shell=new_shell();gimp_painter_canvas_ui_set_visible(shell,TRUE);drain();
  GtkWidget *parent=gtk_box_new(GTK_ORIENTATION_VERTICAL,0),*child=gtk_label_new("Dock child");
  g_object_ref_sink(parent);gtk_container_add(GTK_CONTAINER(parent),child);g_object_ref(child);
  for(int i=0;i<5;++i){
    g_assert_true(gimp_painter_canvas_ui_attach_dock(shell,child));
    g_assert_true(gtk_widget_get_ancestor(child,GIMP_TYPE_DISPLAY_SHELL)==GTK_WIDGET(shell));
    gimp_painter_canvas_ui_detach_dock(shell);g_assert_true(gtk_widget_get_parent(child)==parent);
  }
  gimp_painter_canvas_ui_attach_dock(shell,child);gtk_widget_destroy(parent);
  gimp_painter_canvas_ui_detach_dock(shell);g_assert_null(gtk_widget_get_parent(child));
  g_object_unref(child);g_object_unref(parent);close_shell(shell);
}
static void overlay_tools_and_colors(void) {
  GimpDisplayShell *shell=new_shell();gimp_painter_canvas_ui_set_visible(shell,TRUE);drain();
  GimpToolInfo *info=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-paintbrush-tool"));
  g_assert_nonnull(info);gimp_context_set_tool(gimp_get_user_context(gimp),info);drain();
  GtkWidget *button=find(shell->canvas,"gimp-paintbrush-tool");g_assert_nonnull(button);
  g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
  gtk_button_clicked(GTK_BUTTON(button));drain();
  g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(find(shell->canvas,"gimp-paintbrush-tool"))));
  g_assert_null(find(shell->canvas,"painter-canvas-options"));
  GtkWidget *gui=gimp_tools_get_tool_options_gui(info->tool_options);
  g_assert_true(gtk_widget_get_ancestor(gui,GIMP_TYPE_DISPLAY_SHELL)!=GTK_WIDGET(shell));
  GtkWidget *swatches=find(shell->canvas,"painter-fg-bg"),*editor=find(shell->canvas,"painter-color-editor");
  g_assert_nonnull(swatches);g_assert_true(GIMP_IS_COLOR_NOTEBOOK(editor));
  GeglColor *color=gegl_color_new("rgb(0.125,0.25,0.75)");
  gimp_context_set_background(gimp_get_user_context(gimp),color);
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(swatches),GIMP_ACTIVE_COLOR_BACKGROUND);
  gdouble actual[3],expected[3];
  gegl_color_get_pixel(color,babl_format("R'G'B' double"),expected);
  gegl_color_get_pixel(gimp_color_selector_get_color(GIMP_COLOR_SELECTOR(editor)),babl_format("R'G'B' double"),actual);
  for(int i=0;i<3;i++)g_assert_cmpfloat_with_epsilon(actual[i],expected[i],1e-6);
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(swatches),GIMP_ACTIVE_COLOR_FOREGROUND);
  gimp_context_set_foreground(gimp_get_user_context(gimp),color);drain();
  gegl_color_get_pixel(gimp_color_selector_get_color(GIMP_COLOR_SELECTOR(editor)),babl_format("R'G'B' double"),actual);
  for(int i=0;i<3;i++)g_assert_cmpfloat_with_epsilon(actual[i],expected[i],1e-6);
  g_object_unref(color);
  close_shell(shell);
}
static void overlay_tool_group(void) {
  GimpDisplayShell *shell=new_shell();gimp_painter_canvas_ui_set_visible(shell,TRUE);drain();
  GimpToolInfo *move=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-move-tool"));
  GimpToolInfo *align=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-align-tool"));
  GimpToolInfo *saved=gimp_context_get_tool(gimp_get_user_context(gimp));g_object_ref(saved);
  GimpViewable *mp=gimp_viewable_get_parent(GIMP_VIEWABLE(move)),*ap=gimp_viewable_get_parent(GIMP_VIEWABLE(align));
  GimpContainer *mc=mp?gimp_viewable_get_children(mp):gimp->tool_item_list,*ac=ap?gimp_viewable_get_children(ap):gimp->tool_item_list;
  gint mi=gimp_container_get_child_index(mc,GIMP_OBJECT(move)),ai=gimp_container_get_child_index(ac,GIMP_OBJECT(align));
  g_object_ref(mc);g_object_ref(ac);
  gimp_container_remove(mc,GIMP_OBJECT(move));gimp_container_remove(ac,GIMP_OBJECT(align));
  GimpToolGroup *group=gimp_tool_group_new();GimpContainer *children=gimp_viewable_get_children(GIMP_VIEWABLE(group));
  gimp_container_add(children,GIMP_OBJECT(move));gimp_container_add(children,GIMP_OBJECT(align));gimp_container_add(gimp->tool_item_list,GIMP_OBJECT(group));drain();
  GtkWidget *expander=gtk_widget_get_ancestor(find(shell->canvas,"gimp-align-tool"),GTK_TYPE_EXPANDER);g_assert_nonnull(expander);
  gtk_expander_set_expanded(GTK_EXPANDER(expander),TRUE);drain();
  expander=gtk_widget_get_ancestor(find(shell->canvas,"gimp-align-tool"),GTK_TYPE_EXPANDER);
  gtk_expander_set_expanded(GTK_EXPANDER(expander),FALSE);drain();g_assert_false(gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
  expander=gtk_widget_get_ancestor(find(shell->canvas,"gimp-align-tool"),GTK_TYPE_EXPANDER);gtk_expander_set_expanded(GTK_EXPANDER(expander),TRUE);drain();
  GtkWidget *button=find(shell->canvas,"gimp-align-tool");g_assert_nonnull(button);gtk_button_clicked(GTK_BUTTON(button));drain();
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==align);
  g_assert_true(gimp_tool_group_get_active_tool_info(group)==align);
  gimp_container_remove(children,GIMP_OBJECT(move));gimp_container_remove(children,GIMP_OBJECT(align));gimp_container_remove(gimp->tool_item_list,GIMP_OBJECT(group));
  if(mc==ac&&mi>ai){gimp_container_insert(ac,GIMP_OBJECT(align),ai);gimp_container_insert(mc,GIMP_OBJECT(move),mi);}
  else{gimp_container_insert(mc,GIMP_OBJECT(move),mi);gimp_container_insert(ac,GIMP_OBJECT(align),ai);}
  gimp_context_set_tool(gimp_get_user_context(gimp),saved);g_object_unref(saved);g_object_unref(group);g_object_unref(mc);g_object_unref(ac);drain();close_shell(shell);
}
/* The tool controller uses native GTK parents and a shell BindingStore slot.
 * Keep the actual clicked/expanded widget retained across reentrant close. */
typedef struct {
  GimpDisplayShell *shell;
  GimpToolGroup *group;
  GimpToolInfo *move, *align, *saved, *replacement;
  GimpContainer *mc, *ac;
  gint mi, ai;
  guint calls;
} ToolReentry;
static void tool_reentry_begin(ToolReentry *f) {
  f->shell=new_shell();gimp_painter_canvas_ui_set_visible(f->shell,TRUE);drain();
  f->move=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-move-tool"));
  f->align=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-align-tool"));
  f->saved=g_object_ref(gimp_context_get_tool(gimp_get_user_context(gimp)));
  GimpViewable *mp=gimp_viewable_get_parent(GIMP_VIEWABLE(f->move)),*ap=gimp_viewable_get_parent(GIMP_VIEWABLE(f->align));
  f->mc=g_object_ref(mp?gimp_viewable_get_children(mp):gimp->tool_item_list);
  f->ac=g_object_ref(ap?gimp_viewable_get_children(ap):gimp->tool_item_list);
  f->mi=gimp_container_get_child_index(f->mc,GIMP_OBJECT(f->move));
  f->ai=gimp_container_get_child_index(f->ac,GIMP_OBJECT(f->align));
  gimp_container_remove(f->mc,GIMP_OBJECT(f->move));gimp_container_remove(f->ac,GIMP_OBJECT(f->align));
  f->group=gimp_tool_group_new();
  GimpContainer *children=gimp_viewable_get_children(GIMP_VIEWABLE(f->group));
  gimp_container_add(children,GIMP_OBJECT(f->move));gimp_container_add(children,GIMP_OBJECT(f->align));
  gimp_container_add(gimp->tool_item_list,GIMP_OBJECT(f->group));
  gimp_context_set_tool(gimp_get_user_context(gimp),f->move);
  gimp_tool_group_set_active_tool_info(f->group,f->move);
  gimp_viewable_set_expanded(GIMP_VIEWABLE(f->group),FALSE);drain();
}
static void tool_reentry_end(ToolReentry *f) {
  g_signal_handlers_disconnect_by_data(f->group,f);
  gimp_painter_canvas_ui_close(f->shell);gimp_painter_canvas_ui_close(f->shell);drain();
  GimpContainer *children=gimp_viewable_get_children(GIMP_VIEWABLE(f->group));
  gimp_container_remove(children,GIMP_OBJECT(f->move));gimp_container_remove(children,GIMP_OBJECT(f->align));
  gimp_container_remove(gimp->tool_item_list,GIMP_OBJECT(f->group));
  if(f->mc==f->ac&&f->mi>f->ai) {
    gimp_container_insert(f->ac,GIMP_OBJECT(f->align),f->ai);gimp_container_insert(f->mc,GIMP_OBJECT(f->move),f->mi);
  } else {
    gimp_container_insert(f->mc,GIMP_OBJECT(f->move),f->mi);gimp_container_insert(f->ac,GIMP_OBJECT(f->align),f->ai);
  }
  gimp_context_set_tool(gimp_get_user_context(gimp),f->saved);
  g_object_unref(f->saved);g_object_unref(f->group);g_object_unref(f->mc);g_object_unref(f->ac);
  close_shell(f->shell);
}
static void tool_reentry_close(GObject *emitter,gpointer data) {
  ToolReentry *f=data;(void)emitter;
  if(f->calls++==0)gimp_painter_canvas_ui_close(f->shell);
}
static void tool_reentry_select(GObject *emitter,gpointer data) {
  ToolReentry *f=data;(void)emitter;
  if(f->calls++==0)gimp_context_set_tool(gimp_get_user_context(gimp),f->replacement);
}
static void tool_reentry_inactive_group(ToolReentry *f) {
  /* The standard tool manager follows active-tool changes in the current
   * group. Use a different current tool so this fixture isolates continuation
   * after our notification, rather than that earlier upstream transition. */
  f->replacement=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-paintbrush-tool"));
  g_assert_nonnull(f->replacement);
  gimp_context_set_tool(gimp_get_user_context(gimp),f->replacement);drain();
}
static void tool_click_close(gboolean expanded) {
  ToolReentry f={0};tool_reentry_begin(&f);tool_reentry_inactive_group(&f);
  GtkWidget *button=find(f.shell->canvas,"gimp-align-tool");g_assert_nonnull(button);g_object_ref(button);
  g_signal_connect(f.group,expanded?"expanded-changed":"active-tool-changed",G_CALLBACK(tool_reentry_close),&f);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(f.calls,==,1);
  g_assert_false(gimp_painter_canvas_ui_get_visible(f.shell));
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  /* Disconnected stale controls and pending refreshes must not revive work. */
  gtk_button_clicked(GTK_BUTTON(button));drain();
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  g_object_unref(button);tool_reentry_end(&f);
}
static void tool_click_active_owner_close(void) { tool_click_close(FALSE); }
static void tool_click_expanded_owner_close(void) { tool_click_close(TRUE); }
static gulong canvas_handler (gpointer emitter, const gchar *name);
static void tool_expander_owner_close(void) {
  ToolReentry f={0};tool_reentry_begin(&f);tool_reentry_inactive_group(&f);
  gimp_tool_group_set_active_tool_info(f.group,f.align);drain();
  GtkWidget *button=find(f.shell->canvas,"gimp-align-tool");g_assert_nonnull(button);
  GtkWidget *expander=gtk_widget_get_ancestor(button,GTK_TYPE_EXPANDER);g_assert_nonnull(expander);g_object_ref(expander);
  g_assert_false(gtk_expander_get_expanded(GTK_EXPANDER(expander)));
  gulong expanded_handler=canvas_handler(expander,"notify::expanded");
  g_signal_connect(f.group,"expanded-changed",G_CALLBACK(tool_reentry_close),&f);
  gtk_expander_set_expanded(GTK_EXPANDER(expander),TRUE);
  g_assert_cmpuint(f.calls,==,1);
  g_assert_false(gimp_painter_canvas_ui_get_visible(f.shell));
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  g_assert_false(g_signal_handler_is_connected(expander,expanded_handler));
  gboolean expanded=gimp_viewable_get_expanded(GIMP_VIEWABLE(f.group));
  gtk_expander_set_expanded(GTK_EXPANDER(expander),FALSE);
  gtk_expander_set_expanded(GTK_EXPANDER(expander),TRUE);drain();
  g_assert_cmpint(gimp_viewable_get_expanded(GIMP_VIEWABLE(f.group)),==,expanded);
  g_assert_cmpuint(f.calls,==,1);
  g_object_unref(expander);tool_reentry_end(&f);
}
static void tool_click_newer_selection(void) {
  ToolReentry f={0};tool_reentry_begin(&f);
  f.replacement=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-paintbrush-tool"));
  g_assert_nonnull(f.replacement);
  GtkWidget *button=find(f.shell->canvas,"gimp-align-tool");g_assert_nonnull(button);g_object_ref(button);
  g_signal_connect(f.group,"active-tool-changed",G_CALLBACK(tool_reentry_select),&f);
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_cmpuint(f.calls,==,1);
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  drain();g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  GtkWidget *selected=find(f.shell->canvas,"gimp-paintbrush-tool");g_assert_nonnull(selected);
  g_assert_true(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(selected)));
  g_object_unref(button);tool_reentry_end(&f);
}
static void tool_group_selection_restores_canvas_focus(void) {
  ToolReentry f={0};tool_reentry_begin(&f);
  GtkWidget *entry=gtk_entry_new();g_object_ref_sink(entry);
  g_assert_true(gimp_painter_canvas_ui_attach_dock(f.shell,entry));
  GtkWidget *toplevel=gtk_widget_get_toplevel(GTK_WIDGET(f.shell));g_assert_true(GTK_IS_WINDOW(toplevel));
  gtk_widget_grab_focus(entry);
  g_assert_true(gtk_window_get_focus(GTK_WINDOW(toplevel))==entry);
  GtkWidget *button=find(f.shell->canvas,"gimp-align-tool");g_assert_nonnull(button);g_object_ref(button);
  /* The standard tool manager changes the context synchronously when choosing
   * another member of the current group. That is completion of this choice,
   * not a newer conflicting request, and must still restore canvas focus. */
  gtk_button_clicked(GTK_BUTTON(button));
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.align);
  g_assert_true(gtk_window_get_focus(GTK_WINDOW(toplevel))==f.shell->canvas);
  drain();g_assert_true(gtk_window_get_focus(GTK_WINDOW(toplevel))==f.shell->canvas);
  g_object_unref(button);gimp_painter_canvas_ui_detach_dock(f.shell);gtk_widget_destroy(entry);g_object_unref(entry);
  tool_reentry_end(&f);
}
static void tool_pending_refresh_owner_destroy(void) {
  ToolReentry f={0};tool_reentry_begin(&f);tool_reentry_inactive_group(&f);
  GtkWidget *button=find(f.shell->canvas,"gimp-align-tool");g_assert_nonnull(button);g_object_ref(button);
  GtkWidget *expander=gtk_widget_get_ancestor(button,GTK_TYPE_EXPANDER);g_assert_nonnull(expander);
  GtkWidget *root=gtk_widget_get_parent(expander);g_assert_true(GTK_IS_BOX(root));
  g_assert_cmpint(gtk_orientable_get_orientation(GTK_ORIENTABLE(root)),==,GTK_ORIENTATION_VERTICAL);
  g_assert_nonnull(gtk_widget_get_ancestor(root,GTK_TYPE_SCROLLED_WINDOW));
  g_assert_true(gtk_widget_get_ancestor(root,GIMP_TYPE_DISPLAY_SHELL)==GTK_WIDGET(f.shell));
  gpointer weak_root=root,weak_shell=f.shell;
  g_object_add_weak_pointer(G_OBJECT(root),&weak_root);
  g_object_add_weak_pointer(G_OBJECT(f.shell),&weak_shell);
  g_object_ref(f.shell);
  /* Queue model refresh, then retire its owner without dispatching the idle. */
  gimp_tool_group_set_active_tool_info(f.group,f.align);
  gimp_painter_canvas_ui_close(f.shell);gimp_painter_canvas_ui_close(f.shell);
  gtk_button_clicked(GTK_BUTTON(button));drain();
  g_assert_false(gimp_painter_canvas_ui_get_visible(f.shell));
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  g_object_unref(button);tool_reentry_end(&f);
  g_object_unref(f.shell);drain();
  g_assert_null(weak_shell);g_assert_null(weak_root);
}
static void multiple_canvas_context_and_dock_transfer(void) {
  GimpDisplayShell *a=new_shell();gimp_painter_canvas_ui_set_visible(a,TRUE);drain();
  GimpImage *ia=gimp_display_get_image(a->display);
  GimpLayer *la=gimp_image_get_selected_layers(ia)->data;
  GimpDisplayShell *b=new_shell();gimp_painter_canvas_ui_set_visible(b,TRUE);drain();
  GimpImage *ib=gimp_display_get_image(b->display);
  GimpLayer *lb=gimp_image_get_selected_layers(ib)->data;
  GtkWidget *ta=find(a->canvas,"painter-layer-tiles"),*tb=find(b->canvas,"painter-layer-tiles");
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(ta))==ia);
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(tb))==ib);
  g_assert_true(action_data_get_gimp(ta)==gimp);
  g_assert_true(action_data_get_gimp(tb)==gimp);
  g_assert_true(action_data_get_context(ta)==GIMP_IMAGE_EDITOR(ta)->context);
  g_assert_true(action_data_get_context(tb)==GIMP_IMAGE_EDITOR(tb)->context);
  g_assert_true(action_data_get_context(ta)!=action_data_get_context(tb));
  g_assert_true(action_data_get_image(ta)==ia);
  g_assert_true(action_data_get_image(tb)==ib);
  g_assert_true(action_data_get_display(ta)==a->display);
  g_assert_true(action_data_get_display(tb)==b->display);
  /* Deliberately address the inactive canvas, as the former direct toggle
   * test did. Native mapped-pointer routes have independent event tests. */
  g_assert_true(send_button(layer_canvas(ta),GDK_BUTTON_PRESS,1));drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(la)));
  g_assert_true(gimp_item_get_visible(GIMP_ITEM(lb)));
  GtkWidget *parent=gtk_box_new(GTK_ORIENTATION_VERTICAL,0),*child=gtk_label_new("Transferred dock");
  g_object_ref_sink(parent);gtk_box_pack_end(GTK_BOX(parent),child,FALSE,TRUE,7);g_object_ref(child);
  g_assert_true(gimp_painter_canvas_ui_attach_dock(a,child));
  g_assert_true(gimp_painter_canvas_ui_attach_dock(b,child));
  gimp_painter_canvas_ui_detach_dock(a);
  g_assert_true(gtk_widget_get_ancestor(child,GIMP_TYPE_DISPLAY_SHELL)==GTK_WIDGET(b));
  close_shell(a);
  g_assert_true(send_button(layer_canvas(tb),GDK_BUTTON_PRESS,1));drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(lb)));
  gimp_painter_canvas_ui_detach_dock(b);g_assert_true(gtk_widget_get_parent(child)==parent);
  gboolean expand,fill;guint padding;GtkPackType pack;
  gtk_box_query_child_packing(GTK_BOX(parent),child,&expand,&fill,&padding,&pack);
  g_assert_false(expand);g_assert_true(fill);g_assert_cmpuint(padding,==,7);g_assert_cmpint(pack,==,GTK_PACK_END);
  close_shell(b);gtk_widget_destroy(parent);g_object_unref(child);g_object_unref(parent);
}
static void overlay_owner_destroy_callbacks(void) {
  GimpDisplayShell *shell=new_shell();gimp_painter_canvas_ui_set_visible(shell,TRUE);drain();
  GtkWidget *button=find(shell->canvas,"gimp-paintbrush-tool");g_assert_nonnull(button);g_object_ref(button);
  g_object_ref(shell);close_shell(shell);
  gtk_button_clicked(GTK_BUTTON(button));g_assert_false(gimp_painter_canvas_ui_get_visible(shell));
  g_object_unref(button);g_object_unref(shell);drain();
}
/* 07.008: observe the actual instance connections, not just safe dispatch
 * through an already-closed BindingStore.  Keep native emitters alive while
 * their Canvas owner retires. */
static gulong canvas_handler (gpointer emitter, const gchar *name) {
  guint id; GQuark detail;
  g_assert_true(g_signal_parse_name(name,G_OBJECT_TYPE(emitter),&id,&detail,TRUE));
  gulong handler=g_signal_handler_find(emitter,G_SIGNAL_MATCH_ID|G_SIGNAL_MATCH_DETAIL,
                                      id,detail,NULL,NULL,NULL);
  g_assert_cmpuint(handler,!=,0);return handler;
}
typedef struct {
  GimpDisplayShell *shell;
  GtkWidget *frame,*selector,*swatches,*button;
  gulong enter,leave,color,active,clicked;
  guint destroy_calls;
} CanvasEmitters;
static void canvas_emitters_begin(CanvasEmitters *f) {
  f->shell=new_shell();g_object_ref(f->shell);
  gimp_painter_canvas_ui_set_visible(f->shell,TRUE);drain();
  f->frame=g_object_ref(layer_panel(f->shell)->widget);
  f->selector=g_object_ref(find(f->shell->canvas,"painter-color-editor"));
  f->swatches=g_object_ref(find(f->shell->canvas,"painter-fg-bg"));
  f->button=g_object_ref(find(f->shell->canvas,"gimp-paintbrush-tool"));
  f->enter=canvas_handler(f->frame,"enter-notify-event");
  f->leave=canvas_handler(f->frame,"leave-notify-event");
  f->color=canvas_handler(f->selector,"color-changed");
  f->active=canvas_handler(f->swatches,"notify::active-color");
  f->clicked=canvas_handler(f->button,"clicked");
}
static void canvas_emitters_disconnected(CanvasEmitters *f) {
  g_assert_false(g_signal_handler_is_connected(f->frame,f->enter));
  g_assert_false(g_signal_handler_is_connected(f->frame,f->leave));
  g_assert_false(g_signal_handler_is_connected(f->selector,f->color));
  g_assert_false(g_signal_handler_is_connected(f->swatches,f->active));
  g_assert_false(g_signal_handler_is_connected(f->button,f->clicked));
  GimpContext *context=gimp_get_user_context(gimp);
  GimpToolInfo *tool=gimp_context_get_tool(context);
  GeglColor *fg=gegl_color_duplicate(gimp_context_get_foreground(context));
  GeglColor *bg=gegl_color_duplicate(gimp_context_get_background(context));
  GeglColor *late=gegl_color_new("rgb(0.23,0.61,0.84)");
  /* Emitting a signal remains valid on retained destroyed widgets; calling
   * their disposed native selector implementation would test GTK internals. */
  g_signal_emit_by_name(f->selector,"color-changed",late);
  g_object_notify(G_OBJECT(f->swatches),"active-color");
  GdkEventCrossing crossing={0};gboolean handled=FALSE;
  crossing.type=GDK_ENTER_NOTIFY;
  g_signal_emit_by_name(f->frame,"enter-notify-event",&crossing,&handled);
  crossing.type=GDK_LEAVE_NOTIFY;
  g_signal_emit_by_name(f->frame,"leave-notify-event",&crossing,&handled);
  gtk_button_clicked(GTK_BUTTON(f->button));drain();
  g_assert_true(gimp_context_get_tool(context)==tool);
  gdouble actual[4],expected[4];
  gegl_color_get_pixel(fg,babl_format("R'G'B'A double"),expected);
  gegl_color_get_pixel(gimp_context_get_foreground(context),babl_format("R'G'B'A double"),actual);
  for(guint i=0;i<4;i++)g_assert_cmpfloat_with_epsilon(actual[i],expected[i],1e-9);
  gegl_color_get_pixel(bg,babl_format("R'G'B'A double"),expected);
  gegl_color_get_pixel(gimp_context_get_background(context),babl_format("R'G'B'A double"),actual);
  for(guint i=0;i<4;i++)g_assert_cmpfloat_with_epsilon(actual[i],expected[i],1e-9);
  g_assert_false(gimp_painter_canvas_ui_get_visible(f->shell));
  g_object_unref(fg);g_object_unref(bg);g_object_unref(late);
}
static void canvas_emitters_end(CanvasEmitters *f) {
  /* Destroy is not finalization. Prove that the retired owner's connections
   * and callback leases do not keep any of these actual emitters alive. */
  gpointer emitters[]={f->frame,f->selector,f->swatches,f->button,f->shell};
  gpointer weak[G_N_ELEMENTS(emitters)];
  for(guint i=0;i<G_N_ELEMENTS(emitters);i++) {
    weak[i]=emitters[i];g_object_add_weak_pointer(G_OBJECT(emitters[i]),&weak[i]);
  }
  for(guint i=0;i<G_N_ELEMENTS(emitters);i++)g_object_unref(emitters[i]);
  drain();
  for(guint i=0;i<G_N_ELEMENTS(emitters);i++)g_assert_null(weak[i]);
}
static void canvas_frame_close_reentry(GtkWidget *frame,gpointer data) {
  CanvasEmitters *f=data;(void)frame;f->destroy_calls++;
  /* This runs before child widget teardown can hide an owner leak by
   * independently removing all of its signal handlers. */
  g_assert_false(g_signal_handler_is_connected(f->selector,f->color));
  g_assert_false(g_signal_handler_is_connected(f->swatches,f->active));
  g_assert_false(g_signal_handler_is_connected(f->button,f->clicked));
  gimp_painter_canvas_ui_close(f->shell);gimp_painter_canvas_ui_close(f->shell);
}
static void overlay_retained_emitters_shell_close(void) {
  CanvasEmitters f={0};canvas_emitters_begin(&f);
  g_signal_connect(f.frame,"destroy",G_CALLBACK(canvas_frame_close_reentry),&f);
  close_shell(f.shell);
  g_assert_cmpuint(f.destroy_calls,==,1);
  gimp_painter_canvas_ui_close(f.shell);gimp_painter_canvas_ui_close(f.shell);
  canvas_emitters_disconnected(&f);canvas_emitters_end(&f);
}
static void overlay_emitters_destroy_before_owner(void) {
  CanvasEmitters f={0};canvas_emitters_begin(&f);
  gtk_widget_destroy(f.frame);gtk_widget_destroy(f.selector);
  gtk_widget_destroy(f.swatches);gtk_widget_destroy(f.button);
  /* Emitter destroy can release closures before Connection::close. */
  gimp_painter_canvas_ui_close(f.shell);gimp_painter_canvas_ui_close(f.shell);
  canvas_emitters_disconnected(&f);close_shell(f.shell);canvas_emitters_end(&f);
}
static void canvas_color_close(GimpColorSelector *selector,GeglColor *color,gpointer data) {
  guint *calls=g_object_get_data(G_OBJECT(selector),"canvas-close-calls");
  (void)color;(*calls)++;
  gimp_painter_canvas_ui_close(data);gimp_painter_canvas_ui_close(data);
}
static void overlay_context_color_reentrant_close(void) {
  CanvasEmitters f={0};canvas_emitters_begin(&f);
  GimpContext *context=gimp_get_user_context(gimp);guint calls=0;
  GeglColor *saved=gegl_color_duplicate(gimp_context_get_foreground(context));
  GeglColor *color=gegl_color_new("rgb(0.13,0.72,0.39)");
  g_object_set_data(G_OBJECT(f.selector),"canvas-close-calls",&calls);
  g_signal_connect(f.selector,"color-changed",G_CALLBACK(canvas_color_close),f.shell);
  /* Context -> Canvas sync -> native selector -> reentrant owner close. */
  gimp_context_set_foreground(context,color);
  g_assert_cmpuint(calls,==,1);
  canvas_emitters_disconnected(&f);
  gimp_context_set_foreground(context,saved);drain();
  g_assert_cmpuint(calls,==,1);
  g_object_unref(saved);g_object_unref(color);
  close_shell(f.shell);canvas_emitters_end(&f);
}
/* The reverse color route is independent of context -> selector sync:
 * selector emission calls the context setter, whose subscribers can retire
 * the owner before selector_color_changed resumes. Cover both target colors. */
typedef struct {
  CanvasEmitters *emitters;
  guint calls;
} CanvasColorReentry;
static void canvas_context_close(GimpContext *context,GeglColor *color,gpointer data) {
  CanvasColorReentry *f=data;(void)context;(void)color;
  f->calls++;
  gimp_painter_canvas_ui_close(f->emitters->shell);
  gimp_painter_canvas_ui_close(f->emitters->shell);
}
static void overlay_selector_color_close(gboolean background) {
  CanvasEmitters f={0};canvas_emitters_begin(&f);
  GimpContext *context=gimp_get_user_context(gimp);
  GeglColor *saved=gegl_color_duplicate(background?
    gimp_context_get_background(context):gimp_context_get_foreground(context));
  GeglColor *color=gegl_color_new("rgb(0.37,0.19,0.63)");
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(f.swatches),background?
    GIMP_ACTIVE_COLOR_BACKGROUND:GIMP_ACTIVE_COLOR_FOREGROUND);
  CanvasColorReentry reentry={&f,0};
  gulong handler=g_signal_connect(context,background?"background-changed":"foreground-changed",
                                 G_CALLBACK(canvas_context_close),&reentry);
  g_signal_emit_by_name(f.selector,"color-changed",color);
  g_assert_cmpuint(reentry.calls,==,1);
  gdouble actual[4],expected[4];
  gegl_color_get_pixel(color,babl_format("R'G'B'A double"),expected);
  gegl_color_get_pixel(background?gimp_context_get_background(context):
                      gimp_context_get_foreground(context),babl_format("R'G'B'A double"),actual);
  for(guint i=0;i<4;i++)g_assert_cmpfloat_with_epsilon(actual[i],expected[i],1e-9);
  canvas_emitters_disconnected(&f);
  g_assert_cmpuint(reentry.calls,==,1);
  g_signal_handler_disconnect(context,handler);
  if(background)gimp_context_set_background(context,saved);
  else gimp_context_set_foreground(context,saved);
  g_object_unref(saved);g_object_unref(color);
  close_shell(f.shell);canvas_emitters_end(&f);
}
static void overlay_selector_foreground_reentrant_close(void) { overlay_selector_color_close(FALSE); }
static void overlay_selector_background_reentrant_close(void) { overlay_selector_color_close(TRUE); }
static void overlay_active_color_reentrant_close(void) {
  CanvasEmitters f={0};canvas_emitters_begin(&f);
  GimpContext *context=gimp_get_user_context(gimp);guint calls=0;
  GeglColor *saved=gegl_color_duplicate(gimp_context_get_background(context));
  GeglColor *color=gegl_color_new("rgb(0.79,0.24,0.43)");
  /* Ensure changing the active swatch produces a real selector color change,
   * rather than accidentally passing with an unchanged default color. */
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(f.swatches),GIMP_ACTIVE_COLOR_FOREGROUND);
  gimp_context_set_background(context,color);
  g_object_set_data(G_OBJECT(f.selector),"canvas-close-calls",&calls);
  g_signal_connect(f.selector,"color-changed",G_CALLBACK(canvas_color_close),f.shell);
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(f.swatches),GIMP_ACTIVE_COLOR_BACKGROUND);
  g_assert_cmpuint(calls,==,1);
  canvas_emitters_disconnected(&f);
  gimp_context_set_background(context,saved);drain();
  g_assert_cmpuint(calls,==,1);
  g_object_unref(saved);g_object_unref(color);
  close_shell(f.shell);canvas_emitters_end(&f);
}
static gboolean demo_timeout(gpointer data) { g_main_loop_quit(data);return G_SOURCE_REMOVE; }
static void demo(void) {
  GimpDisplayShell *shell=new_shell();GimpImage *image=gimp_display_get_image(shell->display);
  GimpLayer *background=gimp_image_get_selected_layers(image)->data;
  GeglColor *white=gegl_color_new("white");gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(background)),NULL,white);g_object_unref(white);
  gimp_drawable_update(GIMP_DRAWABLE(background),0,0,gimp_image_get_width(image),gimp_image_get_height(image));
  gimp_object_set_name(GIMP_OBJECT(background),"Paper");
  GimpLayer *group=gimp_group_layer_new(image);gimp_object_set_name(GIMP_OBJECT(group),"Group");gimp_image_add_layer(image,group,NULL,0,FALSE);
  layer(image,group,"Group Ink");gimp_viewable_set_expanded(GIMP_VIEWABLE(group),TRUE);
  layer(image,NULL,"Ink");
  GFile *file=g_file_new_for_path("/workspace/shared/Painter-Canvas-UI-Demo.xcf");gimp_image_set_file(image,file);g_object_unref(file);
  GimpToolInfo *paintbrush=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,"gimp-paintbrush-tool"));
  gimp_context_set_tool(gimp_get_user_context(gimp),paintbrush);
  GimpContext *context=gimp_get_user_context(gimp);
  gimp_context_set_brush(context,GIMP_BRUSH(gimp_brush_get_standard(context)));
  gimp_context_set_opacity(context,1.);
  GeglColor *black=gegl_color_new("black");gimp_context_set_foreground(context,black);g_object_unref(black);
  g_object_set(paintbrush->tool_options,"brush-size",20.,"use-texture",FALSE,NULL);
  GList *selected=g_list_append(NULL,background);gimp_image_set_selected_layers(image,selected);g_list_free(selected);
  gimp_painter_canvas_ui_set_visible(shell,TRUE);gimp_image_flush(image);drain();
  GMainLoop *loop=g_main_loop_new(NULL,FALSE);
  guint timeout=g_timeout_add_seconds(30,demo_timeout,loop);
  g_print("PAINTER_CANVAS_DEMO_READY\n");g_main_loop_run(loop);
  if(g_main_context_find_source_by_id(NULL,timeout))g_source_remove(timeout);
  g_main_loop_unref(loop);close_shell(shell);
}
#include "test-widget-store-native-dnd.inc"

/* 08.017 native edge fragments. Insert after the canvas test helpers, before
 * main(). Extra includes are in native-edge-includes.h; ADD calls are separate.
 * These use production entry points and signal registrations, not copied DnD
 * dispatch or a synthetic ownership store. Runtime execution is parent-owned. */

typedef struct
{
  GtkWidget *source;
  GtkWidget *dest;
  GdkDragContext *drag;
  GimpViewable *item;
  GimpViewable *received;
  guint gets;
  guint drops;
  gint x;
  gint y;
} NativeToolDnd;

static GimpViewable *
native_tool_dnd_get (GtkWidget *widget, GimpContext **context, gpointer data)
{
  NativeToolDnd *f = data;
  g_assert_true (widget == f->source);
  *context = gimp_get_user_context (gimp);
  f->gets++;
  return f->item;
}

static void
native_tool_dnd_drop (GtkWidget *widget, gint x, gint y,
                      GimpViewable *viewable, gpointer data)
{
  NativeToolDnd *f = data;
  g_assert_true (widget == f->dest);
  f->received = viewable;
  f->x = x;
  f->y = y;
  f->drops++;
}

static void
native_tool_dnd_clipboard_get (GtkClipboard *clipboard,
                               GtkSelectionData *selection,
                               guint info, gpointer data)
{
  NativeToolDnd *f = data;
  (void) clipboard;
  g_signal_emit_by_name (f->source, "drag-data-get", f->drag, selection,
                         info, GDK_CURRENT_TIME);
}

static void
native_tool_dnd_clipboard_clear (GtkClipboard *clipboard, gpointer data)
{
  (void) clipboard;
  (void) data;
}

static GtkSelectionData *
native_tool_dnd_payload (NativeToolDnd *f)
{
  GtkTargetEntry target = GIMP_TARGET_TOOL_ITEM;
  GtkClipboard *clipboard = gtk_clipboard_get
    (gdk_atom_intern_static_string ("_GIMP_PAINTER_NATIVE_EDGE_SELECTION"));
  GtkSelectionData *selection;

  /* GTK owns/allocates the real GtkSelectionData. The callback reaches the
   * production registered drag-data-get handler with its numeric info value. */
  g_assert_true (gtk_clipboard_set_with_data (clipboard, &target, 1,
                                             native_tool_dnd_clipboard_get,
                                             native_tool_dnd_clipboard_clear, f));
  selection = gtk_clipboard_wait_for_contents
    (clipboard, gdk_atom_intern_static_string (target.target));
  gtk_clipboard_clear (clipboard);
  g_assert_nonnull (selection);
  return selection;
}

static void
native_tool_dnd_deliver (NativeToolDnd *f, GtkSelectionData *selection,
                         guint info)
{
  /* The registered production receiver dispatches TOOL_ITEM, resolves the
   * same-process object, and decides whether to invoke the client callback.
   * This checks signal-boundary delivery, not physical pointer transport. */
  g_signal_emit_by_name (f->dest, "drag-data-received", f->drag,
                         17, 29, selection, info, GDK_CURRENT_TIME);
}

static void
native_tool_dnd_assert_target (GtkTargetList *list)
{
  GtkTargetEntry *entries;
  gint count;
  guint info = 0;
  GdkAtom atom = gdk_atom_intern_static_string
    ("application/x-gimp-tool-item-name");

  g_assert_nonnull (list);
  g_assert_true (gtk_target_list_find (list, atom, &info));
  g_assert_cmpuint (info, ==, 23);
  entries = gtk_target_table_new_from_list (list, &count);
  g_assert_cmpint (count, ==, 1);
  g_assert_cmpstr (entries[0].target, ==, "application/x-gimp-tool-item-name");
  g_assert_cmpuint (entries[0].flags, ==, GTK_TARGET_SAME_APP);
  g_assert_cmpuint (entries[0].info, ==, 23);
  gtk_target_table_free (entries, count);
}

static void
native_tool_dnd_wire_payload (void)
{
  NativeToolDnd f = { 0 };
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GimpToolGroup *group = gimp_tool_group_new ();
  GimpToolInfo *tool = gimp_tool_info_get_standard (gimp);
  GimpToolInfo *saved_tool = gimp_context_get_tool (gimp_get_user_context (gimp));
  GimpViewable *objects[2] = { GIMP_VIEWABLE (tool), GIMP_VIEWABLE (group) };
  GList *targets = NULL;
  GtkSelectionData *selection;
  GdkAtom target = gdk_atom_intern_static_string
    ("application/x-gimp-tool-item-name");

  g_assert_cmpint (GIMP_DND_TYPE_TOOL_ITEM, ==, 23);
  g_assert_nonnull (tool);
  if (saved_tool)
    g_object_ref (saved_tool);
  gimp_object_set_name (GIMP_OBJECT (group), "native-edge-tool-group");
  gimp_container_add (gimp->tool_item_list, GIMP_OBJECT (group));
  f.source = gtk_event_box_new ();
  f.dest = gtk_event_box_new ();
  gtk_widget_set_size_request (f.source, 30, 30);
  gtk_widget_set_size_request (f.dest, 30, 30);
  gtk_box_pack_start (GTK_BOX (box), f.source, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (box), f.dest, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);
  drain ();
  g_assert_true (gimp_dnd_viewable_source_add
    (f.source, GIMP_TYPE_TOOL_ITEM, native_tool_dnd_get, &f));
  g_assert_true (gimp_dnd_viewable_dest_add
    (f.dest, GIMP_TYPE_TOOL_ITEM, native_tool_dnd_drop, &f));
  /* Re-registration must replace native callback metadata without duplicate
   * signal delivery or duplicate target registration. */
  g_assert_true (gimp_dnd_viewable_source_add
    (f.source, GIMP_TYPE_TOOL_INFO, native_tool_dnd_get, &f));
  g_assert_true (gimp_dnd_viewable_dest_add
    (f.dest, GIMP_TYPE_TOOL_GROUP, native_tool_dnd_drop, &f));
  native_tool_dnd_assert_target (gtk_drag_source_get_target_list (f.source));
  native_tool_dnd_assert_target (gtk_drag_dest_get_target_list (f.dest));
  targets = g_list_append (targets, target);
  f.drag = gdk_drag_begin_for_device
    (gtk_widget_get_window (f.source),
     gdk_seat_get_pointer (gdk_display_get_default_seat
       (gtk_widget_get_display (f.source))), targets);
  g_list_free (targets);
  g_assert_nonnull (f.drag);

  for (guint i = 0; i < G_N_ELEMENTS (objects); i++)
    {
      gchar *expected;
      guint gets = f.gets;
      guint drops = f.drops;
      f.item = objects[i];
      selection = native_tool_dnd_payload (&f);
      expected = g_strdup_printf ("%d:%p:%s", gimp_get_pid (), f.item,
                                  gimp_object_get_name (f.item));
      g_assert_cmpuint (f.gets, ==, gets + 1);
      g_assert_cmpint (gtk_selection_data_get_format (selection), ==, 8);
      g_assert_cmpint (gtk_selection_data_get_length (selection), ==,
                       strlen (expected));
      g_assert_cmpstr ((const gchar *) gtk_selection_data_get_data (selection),
                       ==, expected);
      g_assert_true (gimp_selection_data_get_tool_item (selection, gimp) ==
                     GIMP_TOOL_ITEM (f.item));
      native_tool_dnd_deliver (&f, selection, 23);
      g_assert_cmpuint (f.drops, ==, drops + 1);
      g_assert_true (f.received == f.item);
      g_assert_cmpint (f.x, ==, 17);
      g_assert_cmpint (f.y, ==, 29);
      gtk_selection_data_free (selection);
      g_free (expected);
    }

  /* The drag context's captured viewable must win over a later source value. */
  f.item = GIMP_VIEWABLE (group);
  g_object_set_data (G_OBJECT (f.drag), "gimp-dnd-viewable", tool);
  {
    guint gets = f.gets;
    selection = native_tool_dnd_payload (&f);
    g_assert_cmpuint (f.gets, ==, gets);
    g_assert_true (gimp_selection_data_get_tool_item (selection, gimp) ==
                   GIMP_TOOL_ITEM (tool));
    gtk_selection_data_free (selection);
  }
  g_object_set_data (G_OBJECT (f.drag), "gimp-dnd-viewable", NULL);

  selection = native_tool_dnd_payload (&f);
  {
    guint drops = f.drops;
    gchar *wrong_pid = g_strdup_printf ("%d:%p:%s", gimp_get_pid () + 1,
                                        group, gimp_object_get_name (group));
    gchar *wrong_addr = g_strdup_printf ("%d:0x1:%s", gimp_get_pid (),
                                         gimp_object_get_name (group));
    gchar *wrong_name = g_strdup_printf ("%d:%p:missing-native-tool", gimp_get_pid (),
                                         group);
    const gchar *bad[] = { "not-a-payload", "1:0x1:missing", wrong_pid,
                           wrong_addr, wrong_name, "0:0x0:missing" };
    for (guint i = 0; i < G_N_ELEMENTS (bad); i++)
      {
        gtk_selection_data_set (selection, target, 8,
                                 (const guchar *) bad[i], strlen (bad[i]));
        native_tool_dnd_deliver (&f, selection, 23);
        g_assert_cmpuint (f.drops, ==, drops);
      }
    g_free (wrong_pid);
    g_free (wrong_addr);
    g_free (wrong_name);
    /* Invalid format/UTF-8 reaches both real container lookup attempts. */
    for (guint i = 0; i < 2; i++)
      {
        const guchar bytes[] = { 0xff, 0xff };
        gtk_selection_data_set (selection, target, i ? 8 : 16, bytes, 2);
        g_test_expect_message ("Gimp-Widgets", G_LOG_LEVEL_WARNING,
                               "*received invalid selection data*");
        g_test_expect_message ("Gimp-Widgets", G_LOG_LEVEL_WARNING,
                               "*received invalid selection data*");
        native_tool_dnd_deliver (&f, selection, 23);
        g_test_assert_expected_messages ();
        g_assert_cmpuint (f.drops, ==, drops);
      }
    gtk_selection_data_set (selection, target, 8, NULL, 0);
    native_tool_dnd_deliver (&f, selection, 23);
    g_assert_cmpuint (f.drops, ==, drops);
  }
  gtk_selection_data_free (selection);
  selection = native_tool_dnd_payload (&f);
  {
    guint drops = f.drops;
    native_tool_dnd_deliver (&f, selection, 987);
    g_assert_cmpuint (f.drops, ==, drops);
    gimp_object_set_name (GIMP_OBJECT (group), "renamed-native-edge-tool-group");
    native_tool_dnd_deliver (&f, selection, 23);
    g_assert_cmpuint (f.drops, ==, drops);
    gimp_object_set_name (GIMP_OBJECT (group), "native-edge-tool-group");
  }
  /* Retaining the object itself must not make a payload for a removed model
   * entry valid. Removal is checked by the production receiver. */
  gimp_container_remove (gimp->tool_item_list, GIMP_OBJECT (group));
  {
    guint drops = f.drops;
    native_tool_dnd_deliver (&f, selection, 23);
    g_assert_cmpuint (f.drops, ==, drops);
  }
  gtk_selection_data_free (selection);
  f.item = GIMP_VIEWABLE (tool);
  selection = native_tool_dnd_payload (&f);
  g_assert_true (gimp_dnd_viewable_dest_remove (f.dest, GIMP_TYPE_TOOL_ITEM));
  {
    guint drops = f.drops;
    native_tool_dnd_deliver (&f, selection, 23);
    g_assert_cmpuint (f.drops, ==, drops);
  }
  g_assert_true (gimp_dnd_viewable_source_remove (f.source, GIMP_TYPE_TOOL_ITEM));
  g_assert_null (gimp_dnd_get_drag_viewable (f.source));
  {
    guint gets = f.gets;
    gtk_selection_data_set (selection, target, 8, NULL, 0);
    g_signal_emit_by_name (f.source, "drag-data-get", f.drag, selection,
                           23, GDK_CURRENT_TIME);
    g_assert_cmpuint (f.gets, ==, gets);
    g_assert_cmpint (gtk_selection_data_get_length (selection), ==, 0);
  }
  gtk_selection_data_free (selection);
  gdk_drag_abort (f.drag, GDK_CURRENT_TIME);
  g_object_unref (f.drag);
  gtk_widget_destroy (box);
  g_object_unref (group);
  if (gimp_context_get_tool (gimp_get_user_context (gimp)) != saved_tool)
    gimp_context_set_tool (gimp_get_user_context (gimp), saved_tool);
  g_clear_object (&saved_tool);
  drain ();
}

static guint32
native_surface_pixel (cairo_surface_t *surface, gint x, gint y)
{
  cairo_surface_flush (surface);
  return ((guint32 *) (cairo_image_surface_get_data (surface) +
                        y * cairo_image_surface_get_stride (surface)))[x];
}

static void
native_renderer_background_lifetime (void)
{
  GimpTempBuf *buffer = gimp_temp_buf_new (2, 1, babl_format ("R~G~B~A u8"));
  const guchar pixels[] = { 255, 0, 0, 255, 0, 255, 0, 0 };
  GimpViewRenderer *renderer = gimp_view_renderer_new_full
    (gimp_get_user_context (gimp), GIMP_TYPE_LAYER, 5, 3, 0, FALSE);
  cairo_surface_t *first;
  memcpy (gimp_temp_buf_get_data (buffer), pixels, sizeof (pixels));
  gimp_view_renderer_render_temp_buf (renderer, window, buffer, 1, 1, -1,
                                       GIMP_VIEW_BG_CHECKS, GIMP_VIEW_BG_WHITE);
  g_assert_cmpint (cairo_image_surface_get_width (renderer->surface), ==, 5);
  g_assert_cmpint (cairo_image_surface_get_height (renderer->surface), ==, 3);
  g_assert_cmphex (native_surface_pixel (renderer->surface, 0, 0), ==, 0xffffffff);
  g_assert_cmphex (native_surface_pixel (renderer->surface, 1, 1), ==, 0xffff0000);
  g_assert_cmphex (native_surface_pixel (renderer->surface, 2, 1) >> 24, ==, 0xff);
  first = cairo_surface_reference (renderer->surface);
  gimp_view_renderer_render_temp_buf (renderer, window, buffer, -1, 0, -1,
                                       GIMP_VIEW_BG_WHITE, GIMP_VIEW_BG_WHITE);
  /* Negative offset clips the red pixel and leaves the transparent one over
   * white. Re-rendering replaces the renderer's surface, not retained output. */
  g_assert_cmphex (native_surface_pixel (renderer->surface, 0, 0), ==, 0xffffffff);
  gimp_temp_buf_unref (buffer);
  g_object_unref (renderer);
  g_assert_cmphex (native_surface_pixel (first, 1, 1), ==, 0xffff0000);
  g_assert_cmpint (cairo_surface_status (first), ==, CAIRO_STATUS_SUCCESS);
  cairo_surface_destroy (first);
}

static cairo_surface_t *
native_layer_preview_render (GtkWidget *owner, GimpLayer *target, gboolean want_red)
{
  gint row=0,depth=0;
  GimpImage *image=gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(owner));
  g_assert_true(layer_position(gimp_image_get_layer_iter(image),target,0,&row,&depth));
  gint64 deadline=g_get_monotonic_time()+5*G_TIME_SPAN_SECOND;
  cairo_surface_t *surface=NULL;gboolean matched=FALSE;
  do {
    drain();if(surface)cairo_surface_destroy(surface);
    surface=canvas_snapshot(layer_canvas(owner));
    guint32 pixel=canvas_pixel(surface,depth*8+28,row*64+32);
    matched=want_red?(((pixel>>16)&255)>(pixel&255)+100):((pixel&255)>((pixel>>16)&255)+100);
    if(!matched)g_usleep(1000);
  } while(!matched&&g_get_monotonic_time()<deadline);
  g_assert_true(matched);return surface;
}

static void
native_edge_count_finalized (gpointer data)
{
  /* Unlike weak-pointer notification on dispose, qdata destruction observes
   * release of the object's final native storage. Production never touches
   * this test-only key. The counter remains live until the assertion below. */
  guint *count = data;
  (*count)++;
}

static void
native_layer_preview_pixels_lifetime (void)
{
  GimpImage *image=new_image();GimpLayer *target=layer(image,NULL,"Cached preview pixels");
  GeglColor *red=gegl_color_new("red"),*blue=gegl_color_new("blue");
  gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(target)),NULL,red);
  gimp_drawable_update(GIMP_DRAWABLE(target),0,0,-1,-1);
  GtkWidget *widget=tiles(image);guint canvas_finalized=0,layer_finalized=0;
  cairo_surface_t *red_surface=native_layer_preview_render(widget,target,TRUE);
  gegl_buffer_set_color(gimp_drawable_get_buffer(GIMP_DRAWABLE(target)),NULL,blue);
  gimp_drawable_update(GIMP_DRAWABLE(target),0,0,-1,-1);
  gimp_viewable_invalidate_preview(GIMP_VIEWABLE(target));
  cairo_surface_t *blue_surface=native_layer_preview_render(widget,target,FALSE);
  g_assert_cmphex(canvas_pixel(red_surface,28,32),!=,canvas_pixel(blue_surface,28,32));
  g_object_set_data_full(G_OBJECT(layer_canvas(widget)),"native-edge-canvas-finalization",&canvas_finalized,native_edge_count_finalized);
  g_object_set_data_full(G_OBJECT(target),"native-edge-layer-finalization",&layer_finalized,native_edge_count_finalized);
  /* Pending cache generation must not retain the canvas or its layer. */
  gimp_viewable_invalidate_preview(GIMP_VIEWABLE(target));close_tiles(widget);
  g_assert_cmpuint(canvas_finalized,==,1);
  gimp_viewable_invalidate_preview(GIMP_VIEWABLE(target));drain();
  g_object_unref(image);drain();g_assert_cmpuint(layer_finalized,==,1);
  g_assert_cmpuint((canvas_pixel(red_surface,28,32)>>16)&255,>,canvas_pixel(red_surface,28,32)&255);
  cairo_surface_destroy(red_surface);cairo_surface_destroy(blue_surface);
  g_object_unref(red);g_object_unref(blue);
}

static void
cached_preview_fill (GimpDrawable *drawable, const gchar *name)
{
  GeglColor *color=gegl_color_new(name);
  gegl_buffer_set_color(gimp_drawable_get_buffer(drawable),NULL,color);
  g_object_unref(color);gimp_drawable_update(drawable,0,0,-1,-1);
  gimp_viewable_invalidate_preview(GIMP_VIEWABLE(drawable));
}

static void
renderer_group_cache_and_hit_regions (void)
{
  GimpImage *image=new_image();
  GimpLayer *group=gimp_group_layer_new(image);
  gimp_image_add_layer(image,group,NULL,0,FALSE);
  GimpLayer *child=layer(image,group,"Nested pixels");
  cached_preview_fill(GIMP_DRAWABLE(child),"red");
  gimp_viewable_set_expanded(GIMP_VIEWABLE(group),TRUE);
  GtkWidget *w=tiles(image),*canvas=layer_canvas(w);
  cairo_surface_t *surface=native_layer_preview_render(w,group,TRUE);
  cairo_surface_destroy(surface);
  surface=native_layer_preview_render(w,child,TRUE);
  /* Ancestor guides occupy the child's left gutter, outside its preview. */
  guint32 bar=canvas_pixel(surface,2,96);
  g_assert_cmpuint((bar>>16)&255,==,bar&255);
  g_assert_cmpuint(bar&255,>,150);
  cairo_surface_destroy(surface);
  cached_preview_fill(GIMP_DRAWABLE(child),"blue");
  gimp_image_flush(image);
  surface=native_layer_preview_render(w,group,FALSE);cairo_surface_destroy(surface);
  surface=native_layer_preview_render(w,child,FALSE);cairo_surface_destroy(surface);
  g_assert_true(layer_button(w,child,1,0,FALSE));
  g_assert_true(gimp_image_get_selected_layers(image)->data==child);
  /* A click in the indentation must not toggle the child's eye or select it. */
  g_assert_true(layer_button(w,group,1,0,FALSE));
  g_assert_true(canvas_button(canvas,GDK_BUTTON_PRESS,1,0,4,72));
  g_assert_true(gimp_item_get_visible(GIMP_ITEM(child)));
  g_assert_true(gimp_image_get_selected_layers(image)->data==group);
  /* Use the actual painted disclosure region, not a direct model setter. */
  g_assert_true(canvas_button(canvas,GDK_BUTTON_PRESS,1,0,50,54));drain();
  g_assert_false(gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
  gint width,height;gtk_widget_get_size_request(canvas,&width,&height);
  g_assert_cmpint(height,==,64);g_assert_cmpint(width,==,64);
  cached_preview_fill(GIMP_DRAWABLE(child),"red");gimp_image_flush(image);
  surface=native_layer_preview_render(w,group,TRUE);cairo_surface_destroy(surface);
  g_assert_true(canvas_button(canvas,GDK_BUTTON_PRESS,1,0,50,54));drain();
  g_assert_true(gimp_viewable_get_expanded(GIMP_VIEWABLE(group)));
  gtk_widget_get_size_request(canvas,&width,&height);
  g_assert_cmpint(height,==,128);g_assert_cmpint(width,==,72);
  surface=native_layer_preview_render(w,child,TRUE);cairo_surface_destroy(surface);
  close_tiles(w);g_object_unref(image);
}

static void
renderer_mask_cache_and_release (void)
{
  GimpImage *image=new_image();GimpLayer *target=layer(image,NULL,"Masked pixels");
  cached_preview_fill(GIMP_DRAWABLE(target),"red");
  GimpLayerMask *mask=gimp_layer_create_mask(target,GIMP_ADD_MASK_WHITE,NULL);
  GError *error=NULL;g_assert_nonnull(gimp_layer_add_mask(target,mask,FALSE,&error));g_assert_no_error(error);
  gimp_layer_set_edit_mask(target,FALSE);
  GtkWidget *w=tiles(image);
  cairo_surface_t *surface=native_layer_preview_render(w,target,TRUE);cairo_surface_destroy(surface);
  gimp_layer_set_edit_mask(target,TRUE);drain();
  surface=canvas_snapshot(layer_canvas(w));guint32 white=canvas_pixel(surface,28,32);
  g_assert_cmpuint(white&255,>,240);g_assert_cmpuint((white>>16)&255,>,240);
  cairo_surface_destroy(surface);
  cached_preview_fill(GIMP_DRAWABLE(mask),"black");drain();
  surface=canvas_snapshot(layer_canvas(w));guint32 black=canvas_pixel(surface,28,32);
  g_assert_cmpuint(black&255,<,15);g_assert_cmpuint((black>>16)&255,<,15);
  cairo_surface_destroy(surface);
  guint mask_finalized=0;
  g_object_set_data_full(G_OBJECT(mask),"native-edge-mask-finalization",&mask_finalized,native_edge_count_finalized);
  gimp_viewable_invalidate_preview(GIMP_VIEWABLE(mask));
  gimp_layer_apply_mask(target,GIMP_MASK_DISCARD,FALSE);
  g_assert_null(gimp_layer_get_mask(target));g_assert_cmpuint(mask_finalized,==,0);
  /* Draw before the queued structural refresh: the cached mask is retained,
   * but no longer belongs to the layer.  Rendering must use the layer preview. */
  surface=canvas_snapshot(layer_canvas(w));guint32 red=canvas_pixel(surface,28,32);
  g_assert_cmpuint((red>>16)&255,>,240);g_assert_cmpuint(red&255,<,15);
  cairo_surface_destroy(surface);
  drain();
  g_assert_null(gimp_layer_get_mask(target));g_assert_cmpuint(mask_finalized,==,1);
  surface=native_layer_preview_render(w,target,TRUE);cairo_surface_destroy(surface);
  close_tiles(w);g_object_unref(image);
}

typedef struct { GtkWidget *canvas;GdkDragContext *drag;guint starts; } LayerDragFixture;
static void
layer_drag_started (GtkWidget *canvas,GdkDragContext *drag,gpointer data)
{
  LayerDragFixture *f=data;g_assert_true(canvas==f->canvas);
  f->starts++;f->drag=g_object_ref(drag);
}
static void
layer_drag_payload_get (GtkClipboard *clipboard,GtkSelectionData *selection,guint info,gpointer data)
{
  LayerDragFixture *f=data;(void)clipboard;
  g_signal_emit_by_name(f->canvas,"drag-data-get",f->drag,selection,info,GDK_CURRENT_TIME);
}
static void
layer_drag_payload_clear (GtkClipboard *clipboard,gpointer data)
{
  (void)clipboard;(void)data;
}
static void
renderer_drag_gesture_payload_and_cancel (void)
{
  GimpImage *image=new_image();GimpLayer *b=layer(image,NULL,"B"),*a=layer(image,NULL,"A");
  GtkWidget *w=tiles(image),*canvas=layer_canvas(w);
  GList *selection=g_list_append(g_list_append(NULL,a),b);
  gimp_image_set_selected_layers(image,selection);g_list_free(selection);drain();
  LayerDragFixture f={canvas,NULL,0};
  gulong handler=g_signal_connect_after(canvas,"drag-begin",G_CALLBACK(layer_drag_started),&f);
  g_assert_true(canvas_button(canvas,GDK_BUTTON_PRESS,1,0,28,32));
  g_assert_true(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  g_assert_cmpuint(g_list_length(gimp_image_get_selected_layers(image)),==,2);
  GdkEvent *motion=gdk_event_new(GDK_MOTION_NOTIFY);
  motion->motion.window=g_object_ref(gtk_widget_get_window(canvas));
  motion->motion.send_event=TRUE;motion->motion.time=GDK_CURRENT_TIME;
  motion->motion.state=GDK_BUTTON1_MASK;motion->motion.x=28;motion->motion.y=58;
  gdk_event_set_device(motion,gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(canvas))));
  gtk_widget_event(canvas,motion);gdk_event_free(motion);drain();
  g_assert_cmpuint(f.starts,==,1);g_assert_nonnull(f.drag);
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  g_assert_cmpuint(g_list_length(gimp_image_get_selected_layers(image)),==,2);
  GtkTargetEntry target={(gchar *)"application/x-gimp-painter-layer-tile",GTK_TARGET_SAME_APP,0};
  GdkAtom atom=gdk_atom_intern_static_string(target.target);guint info=99;
  g_assert_true(gtk_target_list_find(gtk_drag_source_get_target_list(canvas),atom,&info));g_assert_cmpuint(info,==,0);
  g_assert_true(gtk_target_list_find(gtk_drag_dest_get_target_list(canvas),atom,&info));
  GtkClipboard *clipboard=gtk_clipboard_get(gdk_atom_intern_static_string("_GIMP_PAINTER_LAYER_DRAG_TEST"));
  g_assert_true(gtk_clipboard_set_with_data(clipboard,&target,1,layer_drag_payload_get,layer_drag_payload_clear,&f));
  GtkSelectionData *payload=gtk_clipboard_wait_for_contents(clipboard,atom);
  g_assert_nonnull(payload);g_assert_cmpint(gtk_selection_data_get_length(payload),==,sizeof(gint));
  gint id=0;memcpy(&id,gtk_selection_data_get_data(payload),sizeof(id));
  g_assert_cmpint(id,==,gimp_item_get_id(GIMP_ITEM(a)));
  gtk_selection_data_free(payload);gtk_clipboard_clear(clipboard);
  gtk_drag_cancel(f.drag);drain();
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  g_assert_cmpuint(g_list_length(gimp_image_get_selected_layers(image)),==,2);
  g_assert_true(gimp_image_get_layer_iter(image)->data==a);
  g_assert_true(gimp_image_get_layer_iter(image)->next->data==b);
  g_signal_handler_disconnect(canvas,handler);g_object_unref(f.drag);
  close_tiles(w);g_object_unref(image);
}

static void
native_tool_icon_preview_lifetime (void)
{
  GimpDisplayShell *shell = new_shell ();
  GimpToolInfo *tool = gimp_context_get_tool (gimp_get_user_context (gimp));
  GtkWidget *button;
  GtkWidget *icon;
  guint icon_finalized = 0;
  const gchar *name;
  GtkIconSize size;
  GtkAllocation allocation;
  cairo_surface_t *surface;
  cairo_t *cr;
  gboolean visible = FALSE;
  gboolean saved_visible = gimp_painter_canvas_ui_get_visible (shell);
  gboolean saved_config_visible;
  gboolean config_visible;

  g_object_get (gimp->config, "painter-canvas-ui", &saved_config_visible, NULL);
  g_assert_nonnull (tool);
  gimp_painter_canvas_ui_set_visible (shell, TRUE);
  drain ();
  button = find (shell->canvas, gimp_object_get_name (tool));
  g_assert_true (GTK_IS_BUTTON (button));
  icon = gtk_button_get_image (GTK_BUTTON (button));
  g_assert_true (GTK_IS_IMAGE (icon));
  g_assert_cmpint (gtk_image_get_storage_type (GTK_IMAGE (icon)), ==, GTK_IMAGE_ICON_NAME);
  gtk_image_get_icon_name (GTK_IMAGE (icon), &name, &size);
  g_assert_cmpstr (name, ==, gimp_viewable_get_icon_name (GIMP_VIEWABLE (tool)));
  /* Reject the generic missing-image fallback. This proves that the named
   * production icon is available and draws nonempty pixels; it does not
   * compare theme-specific raster fidelity to a golden image. */
  g_assert_true (gtk_icon_theme_has_icon (gtk_icon_theme_get_for_screen
    (gtk_widget_get_screen (icon)), name));
  g_assert_cmpint (size, ==, GTK_ICON_SIZE_SMALL_TOOLBAR);
  g_assert_true (gtk_widget_get_mapped (icon));
  gtk_widget_get_allocation (icon, &allocation);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        allocation.width, allocation.height);
  cr = cairo_create (surface);
  gtk_widget_draw (icon, cr);
  g_assert_cmpint (cairo_status (cr), ==, CAIRO_STATUS_SUCCESS);
  for (gint y = 0; y < allocation.height; y++)
    for (gint x = 0; x < allocation.width; x++)
      visible |= (native_surface_pixel (surface, x, y) >> 24) != 0;
  g_assert_true (visible);
  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_object_set_data_full (G_OBJECT (icon), "native-edge-icon-finalization",
                          &icon_finalized, native_edge_count_finalized);
  /* Visibility synchronizes the native action, whose callback also changes
   * the default for later shells. Restore this fixture's state before close. */
  gimp_painter_canvas_ui_set_visible (shell, saved_visible);
  drain ();
  g_assert_cmpint (gimp_painter_canvas_ui_get_visible (shell), ==, saved_visible);
  g_object_get (gimp->config, "painter-canvas-ui", &config_visible, NULL);
  g_assert_cmpint (config_visible, ==, saved_config_visible);
  /* Hiding the panels must retain the icon; final storage releases on close. */
  g_assert_cmpuint (icon_finalized, ==, 0);
  close_shell (shell);
  g_assert_cmpuint (icon_finalized, ==, 1);
}

/* Optional helper restoration tests. Requires session-helper.patch. */
static void
native_session_boolean_literals (void)
{
  for (guint i = 0; i < 32; i++)
    {
      GString *text = g_string_new (NULL);
      GimpConfigWriter *writer = gimp_config_writer_new_from_string (text);
      GimpScanner *scanner;
      gboolean value;
      GError *error = NULL;
      gimp_session_write_boolean (writer, "edge-enabled", TRUE);
      gimp_session_write_boolean (writer, "edge-disabled", FALSE);
      g_assert_true (gimp_config_writer_finish (writer, NULL, &error));
      g_assert_no_error (error);
      g_assert_cmpstr (text->str, ==,
                       "(edge-enabled true)\n(edge-disabled false)\n");
      scanner = gimp_scanner_new_string (text->str, text->len, &error);
      g_assert_nonnull (scanner);
      g_assert_no_error (error);
      g_assert_true (gimp_scanner_parse_token (scanner, G_TOKEN_LEFT_PAREN));
      g_assert_true (gimp_scanner_parse_identifier (scanner, "edge-enabled"));
      g_assert_true (gimp_scanner_parse_boolean (scanner, &value));
      g_assert_true (value);
      g_assert_true (gimp_scanner_parse_token (scanner, G_TOKEN_RIGHT_PAREN));
      g_assert_true (gimp_scanner_parse_token (scanner, G_TOKEN_LEFT_PAREN));
      g_assert_true (gimp_scanner_parse_identifier (scanner, "edge-disabled"));
      g_assert_true (gimp_scanner_parse_boolean (scanner, &value));
      g_assert_false (value);
      g_assert_true (gimp_scanner_parse_token (scanner, G_TOKEN_RIGHT_PAREN));
      g_assert_true (gimp_scanner_parse_token (scanner, G_TOKEN_EOF));
      gimp_scanner_unref (scanner);
      g_string_free (text, TRUE);
    }
}

#define NATIVE_SESSION_DIRECTORY "GIMP_TESTING_PAINTER_SESSION_DIRECTORY"
#define NATIVE_SESSION_PAIR      "GIMP_TESTING_PAINTER_SESSION_PAIR"
#define NATIVE_SESSIONRC         "sessionrc-native-edge"
#define NATIVE_DOCKRC            "dockrc-native-edge"

static gboolean
native_session_expected_hide (void)
{
  const gchar *pair = g_getenv (NATIVE_SESSION_PAIR);

  g_assert_nonnull (pair);
  g_assert_true (strcmp (pair, "0") == 0 || strcmp (pair, "1") == 0);
  return strcmp (pair, "1") == 0;
}

static gboolean
native_session_exit_profile (Gimp *instance, gboolean force, gpointer data)
{
  /* tests.c redirects exit writes to gimpdir-output. This handler is connected
   * before gui_init() installs its exit handler, so all child exit writes stay
   * in the same owned profile. Returning FALSE preserves normal shutdown. */
  g_setenv ("GIMP3_DIRECTORY", g_getenv (NATIVE_SESSION_DIRECTORY), TRUE);
  return FALSE;
}

static void
native_session_prepare_startup (void)
{
  gboolean expected_hide, actual_hide, actual_single;
  const gchar *directory = g_getenv (NATIVE_SESSION_DIRECTORY);

  if (! g_test_subprocess () || ! directory)
    return;

  expected_hide = native_session_expected_hide ();
  g_assert_null (gimp->session_name);
  g_setenv ("GIMP3_DIRECTORY", directory, TRUE);
  g_setenv ("GIMP_TESTING_SESSIONRC_NAME", NATIVE_SESSIONRC, TRUE);
  g_setenv ("GIMP_TESTING_DOCKRC_NAME", NATIVE_DOCKRC, TRUE);
  g_assert_cmpstr (gimp_directory (), ==, directory);

  /* gimp_init_for_gui_testing() has loaded config, but has not run gui_init()
   * or gimp_restore() yet. Set opposite values before production startup calls
   * session_init(), before any live window-mode observer or window exists. */
  g_object_set (gimp->config, "hide-docks", ! expected_hide,
                "single-window-mode", expected_hide, NULL);
  g_object_get (gimp->config, "hide-docks", &actual_hide,
                "single-window-mode", &actual_single, NULL);
  g_assert_cmpint (actual_hide, ==, ! expected_hide);
  g_assert_cmpint (actual_single, ==, expected_hide);
  g_signal_connect (gimp, "exit", G_CALLBACK (native_session_exit_profile), NULL);
}

static void
native_session_check_saved (GFile *file, GFile *dock_file,
                            gboolean expected_hide)
{
  gchar *saved = NULL;
  gsize saved_length;
  GError *error = NULL;
  const gchar *hide_line = expected_hide ? "(hide-docks yes)" : "(hide-docks no)";
  const gchar *single_line = expected_hide ?
    "(single-window-mode no)" : "(single-window-mode yes)";

  g_assert_true (g_file_load_contents (file, NULL, &saved, &saved_length,
                                       NULL, &error));
  g_assert_no_error (error);
  g_assert_nonnull (strstr (saved, hide_line));
  g_assert_nonnull (strstr (saved, single_line));
  g_assert_true (g_file_query_exists (dock_file, NULL));
  g_free (saved);
}

static void
native_session_remove_profile (GFile *file)
{
  GError *error = NULL;

  /* Normal child shutdown may also create controllerrc, modifiersrc and other
   * profile files. Remove only the newly created owned directory, never follow
   * symlinks when recursively removing its children. */
  if (g_file_query_file_type (file, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL) ==
      G_FILE_TYPE_DIRECTORY)
    {
      GFileEnumerator *children;
      GFileInfo *info;

      children = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
                                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                            NULL, &error);
      g_assert_no_error (error);
      g_assert_nonnull (children);
      while ((info = g_file_enumerator_next_file (children, NULL, &error)))
        {
          GFile *child = g_file_get_child (file, g_file_info_get_name (info));
          native_session_remove_profile (child);
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
native_session_boolean_reader_writer (void)
{
  if (g_test_subprocess ())
    {
      gboolean expected_hide = native_session_expected_hide ();
      gboolean expected_single = ! expected_hide;
      gboolean actual_hide, actual_single;
      GFile *file = gimp_directory_file (NATIVE_SESSIONRC, NULL);
      GFile *dock_file = gimp_directory_file (NATIVE_DOCKRC, NULL);

      /* Production startup already called the real session_init() once. Let
       * its deferred dock restoration finish before observing/saving the GUI.
       * Do not invoke the startup parser again on a populated dialog factory. */
      gimp_test_run_mainloop_until_idle ();
      g_object_get (gimp->config, "hide-docks", &actual_hide,
                    "single-window-mode", &actual_single, NULL);
      g_assert_cmpint (actual_hide, ==, expected_hide);
      g_assert_cmpint (actual_single, ==, expected_single);
      session_save (gimp, TRUE);
      native_session_check_saved (file, dock_file, expected_hide);
      g_assert_cmpint (gimp_container_get_n_children (global_recent_docks), ==, 0);
      g_object_unref (file);
      g_object_unref (dock_file);
      return;
    }

  {
    gchar *saved_directory = g_strdup (g_getenv (NATIVE_SESSION_DIRECTORY));
    gchar *saved_pair = g_strdup (g_getenv (NATIVE_SESSION_PAIR));
    gboolean saved_hide, saved_single, actual_hide, actual_single;
    GError *error = NULL;

    g_object_get (gimp->config, "hide-docks", &saved_hide,
                  "single-window-mode", &saved_single, NULL);
    for (guint i = 0; i < 2; i++)
      {
        gboolean expected_hide = i != 0;
        gboolean expected_single = i == 0;
        gchar *directory = g_dir_make_tmp ("gimp-painter-session-edge-XXXXXX", &error);
        GFile *directory_file;
        GFile *file;
        GFile *dock_file;
        GString *text = g_string_new (NULL);
        GimpConfigWriter *writer = gimp_config_writer_new_from_string (text);

        g_assert_no_error (error);
        g_assert_nonnull (directory);
        directory_file = g_file_new_for_path (directory);
        file = g_file_get_child (directory_file, NATIVE_SESSIONRC);
        dock_file = g_file_get_child (directory_file, NATIVE_DOCKRC);
        g_assert_false (g_file_query_exists (file, NULL));
        g_assert_false (g_file_query_exists (dock_file, NULL));
        gimp_session_write_boolean (writer, "hide-docks", expected_hide);
        gimp_session_write_boolean (writer, "single-window-mode", expected_single);
        g_assert_true (gimp_config_writer_finish (writer, NULL, &error));
        g_assert_no_error (error);
        g_assert_true (g_file_replace_contents (file, text->str, text->len,
                                                NULL, FALSE, G_FILE_CREATE_NONE,
                                                NULL, NULL, &error));
        g_assert_no_error (error);
        g_string_free (text, TRUE);

        g_setenv (NATIVE_SESSION_DIRECTORY, directory, TRUE);
        g_setenv (NATIVE_SESSION_PAIR, expected_hide ? "1" : "0", TRUE);
        /* Fresh process one reads helper true/false and saves real yes/no.
         * Fresh process two starts from that actual saved session/dock pair.
         * Re-exec avoids initializing multiple Gimp instances in one process. */
        for (guint pass = 0; pass < 2; pass++)
          {
            g_test_trap_subprocess (NULL, 30 * G_USEC_PER_SEC,
                                    (GTestSubprocessFlags) 0);
            g_test_trap_assert_passed ();
            native_session_check_saved (file, dock_file, expected_hide);
          }

        g_assert_true (g_file_delete (file, NULL, &error));
        g_assert_no_error (error);
        g_assert_true (g_file_delete (dock_file, NULL, &error));
        g_assert_no_error (error);
        native_session_remove_profile (directory_file);
        g_object_unref (file);
        g_object_unref (dock_file);
        g_object_unref (directory_file);
        g_free (directory);
      }
    if (saved_directory)
      g_setenv (NATIVE_SESSION_DIRECTORY, saved_directory, TRUE);
    else
      g_unsetenv (NATIVE_SESSION_DIRECTORY);
    if (saved_pair)
      g_setenv (NATIVE_SESSION_PAIR, saved_pair, TRUE);
    else
      g_unsetenv (NATIVE_SESSION_PAIR);
    g_free (saved_directory);
    g_free (saved_pair);
    /* All real parser/config mutations belonged to the fresh children. */
    g_object_get (gimp->config, "hide-docks", &actual_hide,
                  "single-window-mode", &actual_single, NULL);
    g_assert_cmpint (actual_hide, ==, saved_hide);
    g_assert_cmpint (actual_single, ==, saved_single);
  }
}

#include "test-painter-right-docks.inc"
#include "test-painter-right-docks-width.inc"
#include "test-painter-right-docks-conversion.inc"
#include "test-painter-right-host-interactions.inc"

#include "test-painter-legacy-layout.inc"
#include "test-painter-layer-signal-lifetimes.inc"

int main(int argc,char **argv) {
  const gchar *registration_group = g_getenv ("GIMP_TEST_PAINTER_CANVAS_GROUP");

  g_test_init(&argc,&argv,NULL);
  /* Meson splits this harness without renaming, skipping or duplicating cases.
   * An unset group retains the full manual invocation; reject misspellings. */
  g_assert_true (registration_group == NULL ||
                 g_str_equal (registration_group, "original") ||
                 g_str_equal (registration_group, "right-docks") ||
                 g_str_equal (registration_group, "dock-session") ||
                 g_str_equal (registration_group, "dock-input") ||
                 g_str_equal (registration_group, "dock-layout"));
  if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  native_session_prepare_startup ();
  window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);gtk_window_set_default_size(GTK_WINDOW(window),220,700);
#define ADD(f) do { static const CanvasTestCase test_case = { f }; \
  g_test_add_data_func("/painter-canvas-ui/" #f, &test_case, run_canvas_case); } while (0)
  if(g_getenv("PAINTER_CANVAS_DEMO")) { ADD(demo); } else {
  if (registration_group == NULL ||
      g_str_equal (registration_group, "right-docks")) {
ADD(right_docks_native_notebook_transfer);
ADD(right_docks_switch_repeat_and_restore);
ADD(right_docks_hide_restore);
ADD(right_docks_cancel_on_remove);
ADD(right_docks_newer_selection_on_remove);
ADD(right_docks_close_active_tab);
ADD(right_docks_close_inactive_tab);
ADD(right_docks_options_coexist);
ADD(right_docks_first_opt_in_switch);
  }
  if (registration_group == NULL ||
      g_str_equal (registration_group, "dock-session")) {
ADD(right_docks_session_width_unchanged_restore);
ADD(right_docks_session_width_resize_restore);
ADD(right_docks_session_aux_positive_width);
ADD(right_docks_session_aux_legacy_position);
ADD(right_docks_session_aux_deferred_reborrow);
ADD(right_docks_session_aux_deferred_close);
ADD(right_docks_window_mode_roundtrip);
  }
  if (registration_group == NULL ||
      g_str_equal (registration_group, "dock-input")) {
ADD(right_docks_panel_hover);
ADD(right_docks_panel_retreat_input);
ADD(right_docks_panel_release_recovery);
ADD(right_docks_panel_proximity_recovery);
ADD(right_docks_panel_grab_recovery);
ADD(right_docks_panel_focus_recovery);
  }
  if (registration_group == NULL ||
      g_str_equal (registration_group, "dock-layout")) {
ADD(right_docks_controls_reachable_1240);
ADD(right_docks_controls_reachable_1280);
ADD(right_docks_controls_reachable_portrait);
  }
  if (registration_group == NULL ||
      g_str_equal (registration_group, "original")) {
ADD(w08017_dnd_same_parent_order);
ADD(w08017_dnd_cross_parent_last_owner);
ADD(w08017_dnd_reject_self_ancestor);
ADD(w08017_dnd_layer_group_move_undo);
ADD(w08017_dnd_cross_image_copy);
ADD(w08017_dnd_multi_into_before);
ADD(w08017_dnd_multi_into_after);
ADD(w08017_dnd_multi_same_parent_before);
ADD(w08017_dnd_multi_same_parent_after);
ADD(w08017_dnd_multi_cross_parent_before);
ADD(w08017_dnd_multi_cross_parent_after);
ADD(w08017_dnd_multi_caller_topology);
ADD(w08017_dnd_multi_selected_anchor_before);
ADD(w08017_dnd_multi_selected_anchor_after);
ADD(w08017_dnd_multi_transfer_lifetime);
ADD(w08017_dnd_multi_reject_unowned_ancestor);
ADD(w08017_dnd_multi_destination_lost_on_remove);
ADD(w08017_dnd_multi_destination_lost_on_insert);
ADD(w08017_dnd_multi_reject_group_into_before);
ADD(w08017_dnd_multi_reject_group_into_after);
ADD(w08017_dnd_relocate_parent_valid);
ADD(w08017_dnd_relocate_parent_lost);
ADD(w08017_dnd_relocate_root_valid);
ADD(w08017_dnd_relocate_root_lost);
ADD(w08017_dnd_relocate_foreign_root_valid);
ADD(w08017_dnd_relocate_foreign_root_lost);
ADD(w08017_dnd_relocate_later_foreign_valid);
ADD(w08017_dnd_relocate_later_foreign_lost);
ADD(w08017_dnd_relocate_on_insert_valid);
ADD(w08017_dnd_relocate_on_insert_lost);
ADD(w08017_dnd_relocate_on_rollback);
ADD(w08017_dnd_relocate_same_destination);
ADD(w08017_dnd_weak_admission_keeps_move);
ADD(w08017_dnd_relocate_later_on_cleanup);
  ADD(native_tool_dnd_wire_payload);
  ADD(native_renderer_background_lifetime);
  ADD(native_layer_preview_pixels_lifetime);
  ADD(legacy_tabs_and_collection);ADD(legacy_options_repeated_open);
  ADD(renderer_group_cache_and_hit_regions);
  ADD(renderer_mask_cache_and_release);
  ADD(renderer_drag_gesture_payload_and_cancel);
  ADD(native_tool_icon_preview_lifetime);
  ADD(native_session_boolean_literals);
  ADD(native_session_boolean_reader_writer);
  ADD(renderer_native_size_draw);ADD(renderer_native_activation_routes);ADD(renderer_selection_reentry);ADD(renderer_escape_owner_close);ADD(renderer_stale_preview_disconnected);ADD(renderer_modifier_activation);ADD(renderer_selection_same_target);
  ADD(layer_popup_signal_close);ADD(layer_popup_emitter_first);ADD(layer_popup_signal_reentry);ADD(layer_permanent_signal_close);ADD(layer_preset_signal_owner);
  ADD(channel_preview_components);ADD(selection_and_visibility);ADD(group_move_and_undo);ADD(image_switch_disconnect);ADD(long_press_cancel_destroy);ADD(preview_pending_destroy);ADD(popup_repeat_and_owner_close);
  ADD(popup_native_events_and_parent);ADD(popup_destroy_orders);
  ADD(popup_properties_newer_same_image);ADD(popup_creation_newer_same_image);
  ADD(popup_properties_reentrant_owner_close);ADD(popup_creation_reentrant_owner_close);ADD(popup_properties_reentrant_image_switch);ADD(popup_creation_reentrant_image_switch);
  ADD(popup_action_image_switch);ADD(popup_action_newer_same_image);ADD(popup_action_context_switch);ADD(popup_action_owner_close);ADD(popup_action_normal_group);ADD(popup_action_layer_menu_switch);
  ADD(destroyed_tiles_reject_image);ADD(image_switch_popup_owner_close);ADD(image_switch_popup_newer_image);ADD(parent_image_notify_owner_close);
  ADD(popup_controls_and_clone);ADD(mask_controls);ADD(long_press_once);ADD(reentrant_visibility_close);ADD(multiple_reorder_one_undo);ADD(hide_cancels_grab);
  ADD(overlay_topmost_visible_input);ADD(overlay_input_and_restore);ADD(dock_restore_and_parent_close);ADD(overlay_tools_and_colors);ADD(overlay_tool_group);ADD(tool_click_active_owner_close);ADD(tool_click_expanded_owner_close);ADD(tool_expander_owner_close);ADD(tool_click_newer_selection);ADD(tool_pending_refresh_owner_destroy);ADD(tool_group_selection_restores_canvas_focus);ADD(multiple_canvas_context_and_dock_transfer);ADD(overlay_owner_destroy_callbacks);ADD(overlay_retained_emitters_shell_close);ADD(overlay_emitters_destroy_before_owner);ADD(overlay_context_color_reentrant_close);ADD(overlay_selector_foreground_reentrant_close);ADD(overlay_selector_background_reentrant_close);ADD(overlay_active_color_reentrant_close);
  }
  }
  g_application_run(gimp->app,0,NULL);
  int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  gtk_widget_destroy(window);g_object_unref(window);
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
