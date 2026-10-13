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
#include "display/gimppaintercanvasui.h"
#include "painter/gimp-painter-binding.h"
#include "actions/actions-types.h"
#include "actions/actions.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
#include "gimpcoreapp.h"
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
  if (GTK_IS_CONTAINER(w)) { GList *children=gtk_container_get_children(GTK_CONTAINER(w)); GtkWidget *result=NULL;
    for(GList *i=children;i && !result;i=i->next) result=find(i->data,name); g_list_free(children); return result; }
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
static void selection_and_visibility(void) {
  GimpImage *image=new_image(); GimpLayer *a=layer(image,NULL,"A"),*b=layer(image,NULL,"B");
  GtkWidget *w=tiles(image),*visible=find(w,"painter-layer-visible");
  g_assert_nonnull(visible); g_assert_true(gimp_item_get_visible(GIMP_ITEM(b)));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(visible),FALSE); drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(b))); g_assert_true(gimp_image_undo(image)); drain();
  GList *sel=g_list_append(g_list_append(NULL,a),b); gimp_image_set_selected_layers(image,sel);g_list_free(sel); drain();
  GtkWidget *r=find(w,"painter-layer-tile");GtkWidget *l=gtk_widget_get_parent(r);
  GList *selected=gtk_list_box_get_selected_rows(GTK_LIST_BOX(l));g_assert_cmpuint(g_list_length(selected),==,2);g_list_free(selected);
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
  GtkWidget *v=find(w,"painter-layer-visible");gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(v),FALSE);drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(lb)));
  gimp_context_set_image(gimp_get_user_context(gimp),a);drain();
  g_assert_true(gimp_image_editor_get_image(GIMP_IMAGE_EDITOR(w))==b);
  gimp_context_set_image(gimp_get_user_context(gimp),NULL);close_tiles(w);g_object_unref(a);g_object_unref(b);
}
static gboolean send_button(GtkWidget *w,GdkEventType type,guint button) {
  GdkEventButton ev={0};gboolean handled=FALSE;ev.type=type;ev.button=button;ev.x=8;ev.y=8;
  g_signal_emit_by_name(w,type==GDK_BUTTON_PRESS?"button-press-event":"button-release-event",&ev,&handled);return handled;
}
/* 08.016: native composition replaces the old renderer vtable and callbacks. */
static GtkWidget *renderer_preview_for(GtkWidget *w,GimpLayer *layer) {
  if(GIMP_IS_VIEW(w) && GIMP_VIEW(w)->renderer->viewable==GIMP_VIEWABLE(layer))
    return gtk_widget_get_ancestor(w,GTK_TYPE_EVENT_BOX);
  if(GTK_IS_CONTAINER(w)) {
    GList *children=gtk_container_get_children(GTK_CONTAINER(w));GtkWidget *result=NULL;
    for(GList *i=children;i&&!result;i=i->next)result=renderer_preview_for(i->data,layer);
    g_list_free(children);return result;
  }
  return NULL;
}
static void renderer_wait_viewable(GtkWidget *w) {
  gint64 deadline=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
  while((!gtk_widget_get_mapped(w)||!gdk_window_is_viewable(gtk_widget_get_window(w)))&&g_get_monotonic_time()<deadline) {
    g_main_context_iteration(NULL,FALSE);g_usleep(1000);
  }
  g_assert_true(gtk_widget_get_realized(w));g_assert_true(gtk_widget_get_mapped(w));
  g_assert_true(gdk_window_is_viewable(gtk_widget_get_window(w)));
}
static gboolean renderer_button_event_state(GtkWidget *w,guint button,GdkModifierType state) {
  renderer_wait_viewable(w);
  GdkEvent *event=gdk_event_new(GDK_BUTTON_PRESS);
  event->button.window=g_object_ref(gtk_widget_get_window(w));
  event->button.send_event=TRUE;event->button.time=GDK_CURRENT_TIME;
  event->button.button=button;event->button.state=state;event->button.x=8.;event->button.y=8.;
  gdk_event_set_device(event,gdk_seat_get_pointer(gdk_display_get_default_seat(gtk_widget_get_display(w))));
  gboolean handled=gtk_widget_event(w,event);gdk_event_free(event);return handled;
}
static gboolean renderer_button_event(GtkWidget *w,guint button) { return renderer_button_event_state(w,button,0); }
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
  g_assert_true(GTK_IS_POPOVER(popup));
  g_assert_true(gtk_popover_get_relative_to(GTK_POPOVER(popup))==owner);
  g_assert_true(gtk_widget_get_mapped(popup));return popup;
}
static gboolean renderer_count_draw(GtkWidget *w,cairo_t *cr,gpointer data) {
  (void)w;(void)cr;(*(guint *)data)++;return FALSE;
}
static void renderer_native_size_draw(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"Native renderer");GtkWidget *w=tiles(image);
  GtkWidget *preview=renderer_preview_for(w,a),*button=find(w,"layer-popup");
  g_assert_nonnull(preview);g_assert_true(GTK_IS_BUTTON(button));
  GtkWidget *widgets[]={button,preview,gtk_widget_get_ancestor(preview,GTK_TYPE_LIST_BOX_ROW)};
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
  GtkWidget *w=tiles(image),*preview=renderer_preview_for(w,a);g_assert_nonnull(preview);
  for(guint route=0;route<4;route++) {
    if(route==0)g_assert_true(renderer_button_event(preview,3));
    else if(route==1)gtk_button_clicked(GTK_BUTTON(find(w,"layer-popup")));
    else g_assert_true(renderer_key_event(w,route==2?GDK_KEY_Menu:GDK_KEY_F10,route==2?0:GDK_SHIFT_MASK));
    drain();GtkWidget *popup=renderer_current_popup(w);
    GList *selection=gimp_image_get_selected_layers(image);g_assert_cmpuint(g_list_length(selection),==,1);g_assert_true(selection->data==a);
    GtkWidget *opacity=find(popup,"painter-layer-opacity");g_assert_nonnull(opacity);
    g_assert_cmpfloat_with_epsilon(gtk_range_get_value(GTK_RANGE(opacity)),37.,.000001);
    gimp_painter_layer_tiles_cancel_interaction(GIMP_PAINTER_LAYER_TILES(w));drain();
    g_assert_false(GTK_IS_POPOVER(gtk_grab_get_current()));
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
  GtkWidget *preview=renderer_preview_for(w,a);g_assert_nonnull(preview);
  g_assert_true(renderer_button_event(preview,3));drain();
  g_assert_cmpuint(f.calls,==,1);g_assert_cmpuint(g_list_length(gimp_image_get_selected_layers(image)),==,1);g_assert_true(gimp_image_get_selected_layers(image)->data==f.newer);
  /* A same-target notification completes the activation; B supersedes it. */
  if(superseded)g_assert_false(GTK_IS_POPOVER(gtk_grab_get_current()));
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
  g_assert_false(GTK_IS_POPOVER(gtk_grab_get_current()));
  g_signal_handlers_disconnect_by_data(w,&f);g_object_unref(w);g_object_unref(image);
}

static void renderer_stale_preview_disconnected(void) {
  GimpImage *a=new_image(),*b=new_image();GimpLayer *old=layer(a,NULL,"Old"),*newer=layer(b,NULL,"New");
  GtkWidget *w=tiles(a),*preview=renderer_preview_for(w,old);g_assert_nonnull(preview);g_object_ref(preview);
  gpointer weak_preview=preview;g_object_add_weak_pointer(G_OBJECT(preview),&weak_preview);
  gimp_painter_layer_tiles_set_image(GIMP_PAINTER_LAYER_TILES(w),b);drain();
  /* Direct signal delivery is deliberate here: a destroyed widget has no GdkWindow. */
  g_assert_false(send_button(preview,GDK_BUTTON_PRESS,3));
  g_assert_false(send_button(preview,GDK_BUTTON_RELEASE,1));drain();
  g_assert_false(GTK_IS_POPOVER(gtk_grab_get_current()));
  g_assert_true(gimp_image_get_selected_layers(b)->data==newer);
  g_assert_false(gimp_painter_layer_tiles_long_press_pending(GIMP_PAINTER_LAYER_TILES(w)));
  g_object_unref(preview);drain();g_assert_null(weak_preview);
  close_tiles(w);g_object_unref(a);g_object_unref(b);
}

static void renderer_modifier_activation(void) {
  GimpImage *image=new_image();GimpLayer *a=layer(image,NULL,"A"),*b=layer(image,NULL,"B");GtkWidget *w=tiles(image);
  GtkWidget *preview=renderer_preview_for(w,a);g_assert_nonnull(preview);
  GdkModifierType states[]={0,GDK_CONTROL_MASK,GDK_SHIFT_MASK,GDK_CONTROL_MASK,GDK_CONTROL_MASK};
  for(guint i=0;i<G_N_ELEMENTS(states);i++) {
    GList *selection=g_list_append(NULL,i==3?a:b);
    if(i==0||i==4)selection=g_list_append(selection,a);
    gimp_image_set_selected_layers(image,selection);g_list_free(selection);drain();
    g_assert_true(renderer_button_event_state(preview,3,states[i]));drain();
    renderer_current_popup(w);selection=gimp_image_get_selected_layers(image);
    g_assert_cmpuint(g_list_length(selection),==,i>=3?1:2);
    if(i!=4)g_assert_nonnull(g_list_find(selection,a));if(i!=3)g_assert_nonnull(g_list_find(selection,b));
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
  for(int i=0;i<5;++i) { GtkWidget *p=gimp_painter_layer_tiles_popup(GIMP_PAINTER_LAYER_TILES(w),FALSE);g_assert_nonnull(p);gtk_popover_popdown(GTK_POPOVER(p));drain(); }
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
    g_assert_true(GTK_IS_POPOVER(popup));g_assert_true(gtk_popover_get_modal(GTK_POPOVER(popup)));
    g_assert_true(gtk_popover_get_relative_to(GTK_POPOVER(popup))==w);
    GtkWidget *scroll=gtk_bin_get_child(GTK_BIN(popup));g_assert_true(GTK_IS_SCROLLED_WINDOW(scroll));
    GtkWidget *viewport=gtk_bin_get_child(GTK_BIN(scroll));g_assert_true(GTK_IS_VIEWPORT(viewport));
    g_assert_true(GTK_IS_BOX(gtk_bin_get_child(GTK_BIN(viewport))));drain();
    g_assert_true(gtk_widget_get_realized(popup));g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
    GtkWidget *opacity=find(popup,"painter-layer-opacity");g_assert_nonnull(opacity);
    gtk_widget_grab_focus(opacity);
    gtk_range_set_value(GTK_RANGE(opacity),70.);
    g_assert_cmpfloat(gimp_layer_get_opacity(selected),==,.7);
    /* Exercise GtkPopover's inherited event handlers, not a synthetic closed signal. */
    g_assert_true(native_popup_key(popup,GDK_KEY_Escape));drain();
    g_assert_false(gtk_widget_get_visible(popup));g_assert_false(gtk_widget_has_grab(popup));
    g_assert_cmpuint(closed,==,cycle*2+1);
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
  GtkWidget *visible=find(w,"painter-layer-visible");g_assert_nonnull(visible);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(visible),FALSE);drain();
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
  GtkWidget *control=find(w,"painter-layer-visible");g_object_ref(control);
  /* The test is the caller of the GTK setter and leases its argument across
   * the deliberately reentrant destruction of its last container owner. */
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(control),FALSE);
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
static void overlay_input_and_restore(void) {
  GimpDisplayShell *shell=new_shell();
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
  GtkWidget *options=find(shell->canvas,"painter-canvas-options");gtk_button_clicked(GTK_BUTTON(options));drain();
  GtkWidget *gui=gimp_tools_get_tool_options_gui(info->tool_options);
  g_assert_true(gtk_widget_get_ancestor(gui,GIMP_TYPE_DISPLAY_SHELL)==GTK_WIDGET(shell));
  gtk_button_clicked(GTK_BUTTON(options));drain();
  g_assert_true(gtk_widget_get_ancestor(gui,GIMP_TYPE_DISPLAY_SHELL)!=GTK_WIDGET(shell));
  GtkWidget *swatches=find(shell->canvas,"painter-fg-bg"),*editor=find(shell->canvas,"painter-color-editor");
  g_assert_nonnull(swatches);g_assert_nonnull(editor);
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(swatches),GIMP_ACTIVE_COLOR_BACKGROUND);
  g_assert_true(GIMP_COLOR_EDITOR(editor)->edit_bg);
  gimp_fg_bg_editor_set_active(GIMP_FG_BG_EDITOR(GIMP_COLOR_EDITOR(editor)->fg_bg),GIMP_ACTIVE_COLOR_FOREGROUND);
  g_assert_cmpint(GIMP_FG_BG_EDITOR(swatches)->active_color,==,GIMP_ACTIVE_COLOR_FOREGROUND);
  GeglColor *color=gegl_color_new("rgb(0.125,0.25,0.75)");gimp_context_set_foreground(gimp_get_user_context(gimp),color);g_object_unref(color);drain();
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
static void tool_expander_owner_close(void) {
  ToolReentry f={0};tool_reentry_begin(&f);tool_reentry_inactive_group(&f);
  gimp_tool_group_set_active_tool_info(f.group,f.align);drain();
  GtkWidget *button=find(f.shell->canvas,"gimp-align-tool");g_assert_nonnull(button);
  GtkWidget *expander=gtk_widget_get_ancestor(button,GTK_TYPE_EXPANDER);g_assert_nonnull(expander);g_object_ref(expander);
  g_assert_false(gtk_expander_get_expanded(GTK_EXPANDER(expander)));
  g_signal_connect(f.group,"expanded-changed",G_CALLBACK(tool_reentry_close),&f);
  gtk_expander_set_expanded(GTK_EXPANDER(expander),TRUE);
  g_assert_cmpuint(f.calls,==,1);
  g_assert_false(gimp_painter_canvas_ui_get_visible(f.shell));
  g_assert_true(gimp_context_get_tool(gimp_get_user_context(gimp))==f.replacement);
  drain();g_object_unref(expander);tool_reentry_end(&f);
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
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(find(ta,"painter-layer-visible")),FALSE);drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(la)));
  g_assert_true(gimp_item_get_visible(GIMP_ITEM(lb)));
  GtkWidget *parent=gtk_box_new(GTK_ORIENTATION_VERTICAL,0),*child=gtk_label_new("Transferred dock");
  g_object_ref_sink(parent);gtk_box_pack_end(GTK_BOX(parent),child,FALSE,TRUE,7);g_object_ref(child);
  g_assert_true(gimp_painter_canvas_ui_attach_dock(a,child));
  g_assert_true(gimp_painter_canvas_ui_attach_dock(b,child));
  gimp_painter_canvas_ui_detach_dock(a);
  g_assert_true(gtk_widget_get_ancestor(child,GIMP_TYPE_DISPLAY_SHELL)==GTK_WIDGET(b));
  close_shell(a);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(find(tb,"painter-layer-visible")),FALSE);drain();
  g_assert_false(gimp_item_get_visible(GIMP_ITEM(lb)));
  gimp_painter_canvas_ui_detach_dock(b);g_assert_true(gtk_widget_get_parent(child)==parent);
  gboolean expand,fill;guint padding;GtkPackType pack;
  gtk_box_query_child_packing(GTK_BOX(parent),child,&expand,&fill,&padding,&pack);
  g_assert_false(expand);g_assert_true(fill);g_assert_cmpuint(padding,==,7);g_assert_cmpint(pack,==,GTK_PACK_END);
  close_shell(b);gtk_widget_destroy(parent);g_object_unref(child);g_object_unref(parent);
}
static void overlay_owner_destroy_callbacks(void) {
  GimpDisplayShell *shell=new_shell();gimp_painter_canvas_ui_set_visible(shell,TRUE);drain();
  GtkWidget *button=find(shell->canvas,"painter-canvas-options");g_object_ref(button);
  g_object_ref(shell);close_shell(shell);
  gtk_button_clicked(GTK_BUTTON(button));g_assert_false(gimp_painter_canvas_ui_get_visible(shell));
  g_object_unref(button);g_object_unref(shell);drain();
}
static gboolean demo_timeout(gpointer data) { g_main_loop_quit(data);return G_SOURCE_REMOVE; }
static void finish_demo(GtkButton *button,gpointer data) { g_main_loop_quit(data); }
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
  GtkWidget *finish=gtk_button_new_with_label("Finish Demo");
  gtk_box_pack_start(GTK_BOX(gtk_widget_get_parent(find(shell->canvas,"painter-canvas-hide"))),finish,FALSE,FALSE,0);
  g_signal_connect(finish,"clicked",G_CALLBACK(finish_demo),loop);gtk_widget_show(finish);
  guint timeout=g_timeout_add_seconds(240,demo_timeout,loop);
  g_print("PAINTER_CANVAS_DEMO_READY\n");g_main_loop_run(loop);
  if(g_main_context_find_source_by_id(NULL,timeout))g_source_remove(timeout);
  g_signal_handlers_disconnect_by_data(finish,loop);g_main_loop_unref(loop);close_shell(shell);
}
int main(int argc,char **argv) {
  g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);gtk_window_set_default_size(GTK_WINDOW(window),220,700);
#define ADD(f) do { static const CanvasTestCase test_case = { f }; \
  g_test_add_data_func("/painter-canvas-ui/" #f, &test_case, run_canvas_case); } while (0)
  if(g_getenv("PAINTER_CANVAS_DEMO")) { ADD(demo); } else {
  ADD(renderer_native_size_draw);ADD(renderer_native_activation_routes);ADD(renderer_selection_reentry);ADD(renderer_escape_owner_close);ADD(renderer_stale_preview_disconnected);ADD(renderer_modifier_activation);ADD(renderer_selection_same_target);
  ADD(channel_preview_components);ADD(selection_and_visibility);ADD(group_move_and_undo);ADD(image_switch_disconnect);ADD(long_press_cancel_destroy);ADD(preview_pending_destroy);ADD(popup_repeat_and_owner_close);
  ADD(popup_native_events_and_parent);ADD(popup_destroy_orders);
  ADD(popup_properties_newer_same_image);ADD(popup_creation_newer_same_image);
  ADD(popup_properties_reentrant_owner_close);ADD(popup_creation_reentrant_owner_close);ADD(popup_properties_reentrant_image_switch);ADD(popup_creation_reentrant_image_switch);
  ADD(destroyed_tiles_reject_image);ADD(image_switch_popup_owner_close);ADD(image_switch_popup_newer_image);ADD(parent_image_notify_owner_close);
  ADD(popup_controls_and_clone);ADD(mask_controls);ADD(long_press_once);ADD(reentrant_visibility_close);ADD(multiple_reorder_one_undo);ADD(hide_cancels_grab);
  ADD(overlay_input_and_restore);ADD(dock_restore_and_parent_close);ADD(overlay_tools_and_colors);ADD(overlay_tool_group);ADD(tool_click_active_owner_close);ADD(tool_click_expanded_owner_close);ADD(tool_expander_owner_close);ADD(tool_click_newer_selection);ADD(tool_pending_refresh_owner_destroy);ADD(tool_group_selection_restores_canvas_focus);ADD(multiple_canvas_context_and_dock_transfer);ADD(overlay_owner_destroy_callbacks);
  }
  g_application_run(gimp->app,0,NULL);
  int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  gtk_widget_destroy(window);g_object_unref(window);
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
