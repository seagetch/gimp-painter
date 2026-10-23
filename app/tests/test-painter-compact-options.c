/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include "painter/gimp-painter-binding.h"
#include "widgets/gimppaintermybrusheditor.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "libgimpbase/gimpbase.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/gimppainterprofile.h"
#include "libgimpwidgets/gimpwidgets.h"
#include "core/core-types.h"
#include "config/gimpcoreconfig.h"
#include "config/gimprc.h"
#include "widgets/widgets-types.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "display/gimppaintercanvasui.h"
#include "widgets/gimpwidgets-utils.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimplist.h"
#include "core/gimpcontext.h"
#include "core/gimptoolinfo.h"
#include "core/gimptooloptions.h"
#include "core/gimpbrush.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimppattern.h"
#include "tools/tools-types.h"
#include "tools/gimp-tools.h"
#include "tools/gimppaintoptions-gui.h"
#include "tools/gimpmoveoptions.h"
#include "tools/gimpselectionoptions.h"
#include "tools/gimpgradientoptions.h"
#include "tools/gimptransformgridoptions.h"
#include "widgets/gimppropwidgets.h"
#include "widgets/gimppaintercompactoptions.h"
#include "widgets/gimpviewablebutton.h"
#include "widgets/gimpbrusheditor.h"
#include "widgets/gimpdataeditor.h"
#include "widgets/gimpdynamicseditor.h"
#include "widgets/gimpview.h"
#include "widgets/gimpviewrenderer.h"
#include "gimpcoreapp.h"
#include "gimp-app-test-utils.h"
#include "tests.h"
static Gimp *gimp;
static GtkWidget *window;
typedef struct { guint destroyed, finalized; } WidgetLifetime;
typedef struct { GtkBox parent; WidgetLifetime *lifetime; } OwnershipBox;
typedef struct { GtkBoxClass parent; } OwnershipBoxClass;
G_DEFINE_TYPE (OwnershipBox, ownership_box, GTK_TYPE_BOX)
static void ownership_box_finalize (GObject *object) {
  OwnershipBox *box=(OwnershipBox *)object;
  if(box->lifetime)box->lifetime->finalized++;
  G_OBJECT_CLASS(ownership_box_parent_class)->finalize(object);
}
static void ownership_box_class_init (OwnershipBoxClass *klass) {
  G_OBJECT_CLASS(klass)->finalize=ownership_box_finalize;
}
static void ownership_box_init (OwnershipBox *box) { box->lifetime=NULL; }
static void ownership_destroyed (GtkWidget *widget,gpointer data) {
  (void)widget;((WidgetLifetime *)data)->destroyed++;
}
static GtkWidget *ownership_widget (WidgetLifetime *lifetime) {
  OwnershipBox *box=g_object_new(ownership_box_get_type(),"orientation",GTK_ORIENTATION_VERTICAL,NULL);
  box->lifetime=lifetime;g_signal_connect(box,"destroy",G_CALLBACK(ownership_destroyed),lifetime);
  return GTK_WIDGET(box);
}
static void drain (void) { gimp_test_run_mainloop_until_idle (); }
static void gather (GtkWidget *w, GPtrArray *result) {
  if (g_ptr_array_find (result,w,NULL)) return;
  g_ptr_array_add (result,w);
  if (GTK_IS_MENU_BUTTON(w)) {
    GtkPopover *p=gtk_menu_button_get_popover(GTK_MENU_BUTTON(w)); if(p)gather(GTK_WIDGET(p),result);
  }
  if(GTK_IS_FRAME(w)) { GtkWidget *label=gtk_frame_get_label_widget(GTK_FRAME(w));if(label)gather(label,result); }
  if(GTK_IS_CONTAINER(w)) {
    GList *children=gtk_container_get_children(GTK_CONTAINER(w));
    for(GList *i=children;i;i=i->next)gather(i->data,result);g_list_free(children);
  }
}
static GtkWidget *find (GtkWidget *root,const char *prop,GType type) {
  GPtrArray *a=g_ptr_array_new();gather(root,a);GtkWidget *result=NULL;
  for(guint n=0;n<a->len&&!result;n++) {
    GtkWidget *w=g_ptr_array_index(a,n);
    if ((!type || G_TYPE_CHECK_INSTANCE_TYPE(w,type)) &&
        (!g_strcmp0(g_object_get_data(G_OBJECT(w),"gimp-widget-property-name"),prop) || !g_strcmp0(gtk_widget_get_name(w),prop))) result=w;
  }
  g_ptr_array_unref(a);return result;
}
static void compact (GtkWidget *gui,GimpToolOptions *options,gboolean enabled,GtkOrientation direction) {
  GError *error=NULL;g_assert_true(gimp_painter_compact_options_set(gui,options,enabled,direction,&error));g_assert_no_error(error);
}
static GimpToolInfo *info (const char *name) {
  GimpToolInfo *result=GIMP_TOOL_INFO(gimp_container_get_child_by_name(gimp->tool_info_list,name));g_assert_nonnull(result);return result;
}
static void inventory_all_registered_tools (void) {
  gimp_set_focused_once(gimp);
  guint tools=0,controls=0;
  for(GList *i=GIMP_LIST(gimp->tool_info_list)->queue->head;i;i=i->next) {
    GimpToolInfo *tool=i->data;GtkWidget *gui=gimp_tools_get_tool_options_gui(tool->tool_options);
    if(!GTK_IS_BOX(gui))continue;
    GPtrArray *original=g_ptr_array_new();gather(gui,original);
    GPtrArray *parents=g_ptr_array_new();for(guint n=0;n<original->len;n++)g_ptr_array_add(parents,gtk_widget_get_parent(g_ptr_array_index(original,n)));
    GList *order=gtk_container_get_children(GTK_CONTAINER(gui));
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);
    GPtrArray *after=g_ptr_array_new();gather(gui,after);
    for(guint n=0;n<original->len;n++)g_assert_true(g_ptr_array_find(after,g_ptr_array_index(original,n),NULL));
    g_assert_cmpint(gtk_orientable_get_orientation(GTK_ORIENTABLE(gui)),==,GTK_ORIENTATION_HORIZONTAL);
    g_test_message("Registered compact %s: %u original widgets retained",gimp_object_get_name(tool),original->len);
    controls+=original->len;tools++;
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_VERTICAL);
    compact(gui,tool->tool_options,FALSE,GTK_ORIENTATION_HORIZONTAL);
    for(guint n=0;n<original->len;n++)g_assert_true(gtk_widget_get_parent(g_ptr_array_index(original,n))==g_ptr_array_index(parents,n));
    GList *restored=gtk_container_get_children(GTK_CONTAINER(gui));g_assert_cmpuint(g_list_length(order),==,g_list_length(restored));
    for(GList *a=order,*b=restored;a;a=a->next,b=b->next)g_assert_true(a->data==b->data);
    g_list_free(order);g_list_free(restored);g_ptr_array_unref(original);g_ptr_array_unref(parents);g_ptr_array_unref(after);
  }
  g_assert_cmpuint(tools,>=,40);g_test_message("%u registered tool GUIs; %u retained native widgets",tools,controls);
}
static void paint_bidirectional_reset_and_resource (void) {
  GimpToolInfo *tool=info("gimp-paintbrush-tool");gimp_context_set_tool(gimp_get_user_context(gimp),tool);drain();
  GtkWidget *gui=gimp_tools_get_tool_options_gui(tool->tool_options);
  GtkWidget *size=find(gui,"brush-size",GTK_TYPE_SPIN_BUTTON),*texture=find(gui,"use-texture",GTK_TYPE_TOGGLE_BUTTON);
  g_assert_nonnull(size);g_assert_nonnull(texture);
  compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);
  g_object_set(tool->tool_options,"brush-size",73.,NULL);g_assert_cmpfloat(gtk_spin_button_get_value(GTK_SPIN_BUTTON(size)),==,73.);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(size),51.);gdouble value=0;g_object_get(tool->tool_options,"brush-size",&value,NULL);g_assert_cmpfloat(value,==,51.);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(texture),TRUE);gboolean enabled=FALSE;g_object_get(tool->tool_options,"use-texture",&enabled,NULL);g_assert_true(enabled);
  GtkWidget *arrow=find(gui,"painter-compact-use-texture",GTK_TYPE_MENU_BUTTON);g_assert_nonnull(arrow);g_assert_true(gtk_widget_get_sensitive(arrow));
  g_object_set(tool->tool_options,"use-texture",FALSE,NULL);g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(texture)));g_assert_false(gtk_widget_get_sensitive(arrow));
  GimpBrush *previous=gimp_context_get_brush(GIMP_CONTEXT(tool->tool_options));g_object_ref(previous);
  GimpBrush *brush=GIMP_BRUSH(gimp_brush_generated_new("Compact resource fixture",GIMP_BRUSH_GENERATED_CIRCLE,9.,2,.5,1.,0.));
  gimp_context_set_brush(GIMP_CONTEXT(tool->tool_options),brush);
  g_object_set(tool->tool_options,"brush-size",17.,NULL);
  GList *siblings=gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(size)));GtkWidget *reset=NULL;
  for(GList *i=siblings;i;i=i->next)if(GTK_IS_BUTTON(i->data)&&!GTK_IS_TOGGLE_BUTTON(i->data))reset=i->data;
  g_list_free(siblings);g_assert_nonnull(reset);gtk_button_clicked(GTK_BUTTON(reset));
  g_object_get(tool->tool_options,"brush-size",&value,NULL);g_assert_cmpfloat(value,==,MAX(gimp_brush_get_width(brush),gimp_brush_get_height(brush)));
  GPtrArray *all=g_ptr_array_new();gather(gui,all);guint matched=0;
  for(guint n=0;n<all->len;n++){GtkWidget *w=g_ptr_array_index(all,n);if(GIMP_IS_VIEWABLE_BUTTON(w)){
    GimpViewableButton *b=GIMP_VIEWABLE_BUTTON(w);if(GIMP_IS_BRUSH(GIMP_VIEW(b->view)->renderer->viewable)){g_assert_true(GIMP_VIEW(b->view)->renderer->viewable==GIMP_VIEWABLE(brush));matched++;}}}
  g_assert_cmpuint(matched,>=,1);g_ptr_array_unref(all);
  gimp_context_set_brush(GIMP_CONTEXT(tool->tool_options),previous);g_object_unref(previous);g_object_unref(brush);
  compact(gui,tool->tool_options,FALSE,GTK_ORIENTATION_HORIZONTAL);g_assert_true(find(gui,"brush-size",GTK_TYPE_SPIN_BUTTON)==size);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(size),29.);g_object_get(tool->tool_options,"brush-size",&value,NULL);g_assert_cmpfloat(value,==,29.);
}
static void popup_repeat (void) {
  gimp_set_focused_once(gimp);
  GimpToolInfo *tool=info("gimp-paintbrush-tool");GtkWidget *gui=gimp_tools_get_tool_options_gui(tool->tool_options);
  GtkWidget *old=gtk_widget_get_parent(gui);if(old)g_object_ref(old);g_object_ref(gui);
  if(old)gtk_container_remove(GTK_CONTAINER(old),gui);gtk_container_add(GTK_CONTAINER(window),gui);gtk_widget_show_all(window);
  for(guint n=0;n<12;n++){
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *button=find(gui,"painter-compact-brush",GTK_TYPE_MENU_BUTTON);g_assert_nonnull(button);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),TRUE);drain();
    g_assert_true(gtk_widget_get_visible(GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(button)))));
    GtkWidget *editor=find(gui,"painter-compact-brush-editor",GIMP_TYPE_BRUSH_EDITOR);g_assert_nonnull(editor);
    g_assert_true(GIMP_DATA_EDITOR(editor)->data==GIMP_DATA(gimp_context_get_brush(GIMP_CONTEXT(tool->tool_options))));
    g_assert_nonnull(GIMP_BRUSH_EDITOR(editor)->radius_data);g_assert_nonnull(GIMP_BRUSH_EDITOR(editor)->spacing_data);
    gimp_painter_compact_options_popdown(gui);drain();compact(gui,tool->tool_options,FALSE,GTK_ORIENTATION_HORIZONTAL);
  }
  gtk_container_remove(GTK_CONTAINER(window),gui);if(old){gtk_container_add(GTK_CONTAINER(old),gui);g_object_unref(old);}g_object_unref(gui);gtk_widget_hide(window);
}
static void compact_popup_wait_viewable(GtkWidget *popup) {
  /* Mapping the widget can precede the frame-clock's GdkWindow remap.
   * GTK discards synthetic press/key events until that window is viewable. */
  gint64 deadline=g_get_monotonic_time()+2*G_TIME_SPAN_SECOND;
  while(!gdk_window_is_viewable(gtk_widget_get_window(popup)) && g_get_monotonic_time()<deadline) {
    g_main_context_iteration(NULL,FALSE);g_usleep(1000);
  }
  g_assert_true(gdk_window_is_viewable(gtk_widget_get_window(popup)));
}
static void compact_popup_event(GtkWidget *popup,GdkEventType type,gdouble x,gdouble y) {
  if(type==GDK_KEY_PRESS || type==GDK_BUTTON_PRESS)compact_popup_wait_viewable(popup);
  GdkEvent *event=gdk_event_new(type);
  GdkSeat *seat=gdk_display_get_default_seat(gtk_widget_get_display(popup));
  if(type==GDK_KEY_PRESS) {
    event->key.window=g_object_ref(gtk_widget_get_window(popup));event->key.send_event=TRUE;
    event->key.time=GDK_CURRENT_TIME;event->key.keyval=GDK_KEY_Escape;
    gdk_event_set_device(event,gdk_seat_get_keyboard(seat));
  } else {
    event->button.window=g_object_ref(gtk_widget_get_window(popup));event->button.send_event=TRUE;
    event->button.time=GDK_CURRENT_TIME;event->button.button=1;event->button.x=x;event->button.y=y;
    gdk_event_set_device(event,gdk_seat_get_pointer(seat));
  }
  gtk_widget_event(popup,event);gdk_event_free(event);
}
static void compact_popup_closed(GtkPopover *popup,gpointer data) { (void)popup;(*(guint *)data)++; }
static void popup_native_events_and_window_destroy(void) {
  gimp_set_focused_once(gimp);
  GimpToolInfo *tool=info("gimp-paintbrush-tool");gboolean old_jitter=FALSE;
  g_object_get(tool->tool_options,"use-jitter",&old_jitter,NULL);
  g_object_set(tool->tool_options,"use-jitter",TRUE,NULL);
  GtkWidget *host=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(host);
  GtkWidget *gui=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);g_object_ref_sink(gui);
  GtkWidget *body=gimp_prop_spin_scale_new(G_OBJECT(tool->tool_options),"jitter-amount",.1,1.,2);
  GtkWidget *frame=gimp_prop_expanding_frame_new(G_OBJECT(tool->tool_options),"use-jitter",NULL,body,NULL);
  gtk_box_pack_start(GTK_BOX(gui),frame,FALSE,FALSE,0);gtk_container_add(GTK_CONTAINER(host),gui);
  compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);gtk_widget_show_all(host);drain();
  GtkWidget *button=find(gui,"painter-compact-use-jitter",GTK_TYPE_MENU_BUTTON);g_assert_nonnull(button);g_object_ref(button);
  GtkWidget *popup=GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(button)));g_assert_nonnull(popup);g_object_ref(popup);
  g_assert_true(gtk_popover_get_relative_to(GTK_POPOVER(popup))==button);g_assert_true(gtk_popover_get_modal(GTK_POPOVER(popup)));
  GtkSettings *settings=gtk_widget_get_settings(host);gboolean animations=FALSE;
  g_object_get(settings,"gtk-enable-animations",&animations,NULL);g_object_set(settings,"gtk-enable-animations",FALSE,NULL);
  guint closed=0;g_signal_connect(popup,"closed",G_CALLBACK(compact_popup_closed),&closed);
  for(guint cycle=0;cycle<3;cycle++) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),TRUE);drain();g_assert_true(gtk_widget_get_visible(popup));
    g_assert_true(gtk_widget_get_realized(popup));g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
    gtk_widget_grab_focus(body);compact_popup_event(popup,GDK_KEY_PRESS,0,0);drain();
    g_assert_false(gtk_widget_get_visible(popup));g_assert_false(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
    g_assert_false(gtk_widget_has_grab(popup));g_assert_cmpuint(closed,==,cycle*2+1);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),TRUE);drain();
    g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
    compact_popup_wait_viewable(popup);
    GtkAllocation allocation;gtk_widget_get_allocation(gtk_bin_get_child(GTK_BIN(popup)),&allocation);
    g_assert_cmpint(allocation.width,>,0);g_assert_cmpint(allocation.height,>,0);
    gdouble x=allocation.x+allocation.width/2.,y=allocation.y+allocation.height/2.;
    compact_popup_event(popup,GDK_BUTTON_PRESS,x,y);compact_popup_event(popup,GDK_BUTTON_RELEASE,x,y);drain();
    g_assert_true(gtk_widget_get_visible(popup));g_assert_cmpuint(closed,==,cycle*2+1);
    compact_popup_event(popup,GDK_BUTTON_PRESS,-10,-10);compact_popup_event(popup,GDK_BUTTON_RELEASE,-10,-10);drain();
    g_assert_false(gtk_widget_get_visible(popup));g_assert_false(gtk_widget_has_grab(popup));g_assert_cmpuint(closed,==,cycle*2+2);
  }
  g_signal_handlers_disconnect_by_data(popup,&closed);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),TRUE);drain();
  g_assert_true(gtk_widget_get_mapped(popup));g_assert_true(gtk_widget_has_grab(popup));
  gpointer weak_gui=gui,weak_popup=popup,weak_button=button;
  g_object_add_weak_pointer(G_OBJECT(gui),&weak_gui);g_object_add_weak_pointer(G_OBJECT(popup),&weak_popup);
  g_object_add_weak_pointer(G_OBJECT(button),&weak_button);
  g_object_set(settings,"gtk-enable-animations",animations,NULL);
  gtk_widget_destroy(host);gtk_widget_destroy(host);drain();
  g_assert_false(gimp_painter_compact_options_get(gui));g_assert_false(gtk_widget_has_grab(popup));
  gtk_button_clicked(GTK_BUTTON(button));g_signal_emit_by_name(popup,"closed");drain();
  g_assert_false(gimp_painter_compact_options_get(gui));
  g_object_unref(button);g_object_unref(popup);g_object_unref(gui);g_object_unref(host);drain();
  g_assert_null(weak_gui);g_assert_null(weak_popup);g_assert_null(weak_button);
  g_object_set(tool->tool_options,"use-jitter",old_jitter,NULL);
}
static void finalized (gpointer data,GObject *object) { *(gboolean*)data=TRUE; }
static void fresh_owner_lifecycle (void) {
  gimp_set_focused_once(gimp);
  GimpToolInfo *tool=info("gimp-paintbrush-tool");
  for(guint n=0;n<24;n++) {
    GtkWidget *gui=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);g_object_ref_sink(gui);
    gboolean gone=FALSE;g_object_weak_ref(G_OBJECT(gui),finalized,&gone);
    GtkWidget *body=gimp_prop_spin_scale_new(G_OBJECT(tool->tool_options),"jitter-amount",.1,1.,2);
    GtkWidget *frame=gimp_prop_expanding_frame_new(G_OBJECT(tool->tool_options),"use-jitter",NULL,body,NULL);
    gtk_box_pack_start(GTK_BOX(gui),frame,TRUE,FALSE,7);
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_destroy(gui);g_assert_false(gimp_painter_compact_options_get(gui));g_object_unref(gui);g_assert_true(gone);
    g_object_set(tool->tool_options,"use-jitter",n%2,"jitter-amount",.2,NULL);
  }
}
typedef struct { GtkWidget *widget; gboolean expand,fill; guint padding; GtkPackType pack; } Packing;
static void assert_packing_restored (GtkWidget *gui,const Packing *saved,guint count,GList *order) {
  for(guint i=0;i<count;i++) {
    gboolean expand,fill;guint padding;GtkPackType pack;
    g_assert_true(gtk_widget_get_parent(saved[i].widget)==gui);
    gtk_box_query_child_packing(GTK_BOX(gui),saved[i].widget,&expand,&fill,&padding,&pack);
    g_assert_cmpint(expand,==,saved[i].expand);g_assert_cmpint(fill,==,saved[i].fill);
    g_assert_cmpuint(padding,==,saved[i].padding);g_assert_cmpint(pack,==,saved[i].pack);
  }
  GList *actual=gtk_container_get_children(GTK_CONTAINER(gui));
  g_assert_cmpuint(g_list_length(actual),==,g_list_length(order));
  for(GList *a=actual,*b=order;a;a=a->next,b=b->next)g_assert_true(a->data==b->data);
  g_list_free(actual);
}
static void mixed_packing_and_owned_destruction (void) {
  GimpToolInfo *tool=info("gimp-paintbrush-tool");
  WidgetLifetime life[5]={{0}};
  GtkWidget *gui=ownership_widget(&life[0]);g_object_ref_sink(gui);
  GtkWidget *first=ownership_widget(&life[1]),*last=ownership_widget(&life[2]);
  GtkWidget *jitter_body=ownership_widget(&life[3]),*texture_body=ownership_widget(&life[4]);
  gtk_box_pack_start(GTK_BOX(jitter_body),gimp_prop_spin_scale_new(G_OBJECT(tool->tool_options),"jitter-amount",.1,1.,2),FALSE,FALSE,0);
  gtk_box_pack_start(GTK_BOX(texture_body),gtk_label_new("Texture details"),FALSE,FALSE,0);
  GtkWidget *jitter=gimp_prop_expanding_frame_new(G_OBJECT(tool->tool_options),"use-jitter",NULL,jitter_body,NULL);
  GtkWidget *texture=gimp_prop_expanding_frame_new(G_OBJECT(tool->tool_options),"use-texture",NULL,texture_body,NULL);
  gtk_box_pack_start(GTK_BOX(gui),first,TRUE,FALSE,3);
  gtk_box_pack_end(GTK_BOX(gui),last,FALSE,TRUE,9);
  gtk_box_pack_end(GTK_BOX(gui),jitter,TRUE,FALSE,7);
  gtk_box_pack_start(GTK_BOX(gui),texture,FALSE,FALSE,11);
  GtkWidget *retained[]={first,last,jitter_body,texture_body};
  for(guint i=0;i<G_N_ELEMENTS(retained);i++)g_object_ref(retained[i]);
  Packing saved[]={{first,TRUE,FALSE,3,GTK_PACK_START},{last,FALSE,TRUE,9,GTK_PACK_END},
    {jitter,TRUE,FALSE,7,GTK_PACK_END},{texture,FALSE,FALSE,11,GTK_PACK_START}};
  GList *order=gtk_container_get_children(GTK_CONTAINER(gui));
  for(guint cycle=0;cycle<3;cycle++) {
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);
    g_assert_cmpint(gtk_orientable_get_orientation(GTK_ORIENTABLE(gui)),==,GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *button=find(gui,"painter-compact-use-jitter",GTK_TYPE_MENU_BUTTON);g_assert_nonnull(button);
    GtkWidget *popup=GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(button)));
    GtkWidget *scroll=gtk_bin_get_child(GTK_BIN(popup));g_assert_true(GTK_IS_SCROLLED_WINDOW(scroll));
    GtkWidget *viewport=gtk_bin_get_child(GTK_BIN(scroll));g_assert_true(GTK_IS_VIEWPORT(viewport));
    GtkWidget *body=gtk_bin_get_child(GTK_BIN(viewport));g_assert_true(GTK_IS_BOX(body));
    g_assert_true(gtk_widget_is_ancestor(jitter,body));
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_VERTICAL);
    g_assert_cmpint(gtk_orientable_get_orientation(GTK_ORIENTABLE(gui)),==,GTK_ORIENTATION_VERTICAL);
    compact(gui,tool->tool_options,FALSE,GTK_ORIENTATION_HORIZONTAL);
    assert_packing_restored(gui,saved,G_N_ELEMENTS(saved),order);
    for(guint i=0;i<G_N_ELEMENTS(life);i++){g_assert_cmpuint(life[i].destroyed,==,0);g_assert_cmpuint(life[i].finalized,==,0);}
  }
  g_list_free(order);gtk_widget_destroy(gui);
  for(guint i=0;i<G_N_ELEMENTS(life);i++){g_assert_cmpuint(life[i].destroyed,==,1);g_assert_cmpuint(life[i].finalized,==,0);}
  g_object_unref(gui);
  for(guint i=0;i<G_N_ELEMENTS(retained);i++)g_object_unref(retained[i]);
  for(guint i=0;i<G_N_ELEMENTS(life);i++)g_assert_cmpuint(life[i].finalized,==,1);
}
static void native_viewport_composition (void) {
  for(guint scrollable=0;scrollable<2;scrollable++) {
    WidgetLifetime life={0};GtkWidget *scroll=gtk_scrolled_window_new(NULL,NULL);g_object_ref_sink(scroll);
    GtkWidget *child=scrollable?gtk_tree_view_new():ownership_widget(&life);
    gtk_container_add(GTK_CONTAINER(scroll),child);g_object_ref(child);
    GtkWidget *native=gtk_bin_get_child(GTK_BIN(scroll));
    if(scrollable){g_assert_true(GTK_IS_SCROLLABLE(child));g_assert_true(native==child);}
    else {g_assert_true(GTK_IS_VIEWPORT(native));g_assert_true(gtk_bin_get_child(GTK_BIN(native))==child);}
    gtk_widget_destroy(scroll);g_object_unref(scroll);
    if(!scrollable) {
      /* GtkScrolledWindow removes the auto-viewport's child before destroying
       * that viewport. An independently retained child survives unparented. */
      g_assert_null(gtk_widget_get_parent(child));
      g_assert_cmpuint(life.destroyed,==,0);g_assert_cmpuint(life.finalized,==,0);
      gtk_widget_destroy(child);g_assert_cmpuint(life.destroyed,==,1);
    }
    g_object_unref(child);
    if(!scrollable)g_assert_cmpuint(life.finalized,==,1);
  }
}
static void registered_popup_contracts (void) {
  const char *cases[][3]={
    {"gimp-rect-select-tool","fixed-center","painter-compact-rectangle"},
    {"gimp-rect-select-tool","corner-radius","painter-compact-round-corners"},
    {"gimp-paintbrush-tool","jitter-amount","painter-compact-use-jitter"},
    {"gimp-paintbrush-tool","brush-angle","painter-compact-brush"},
    {"gimp-gradient-tool","supersample-depth","painter-compact-supersample"},
    {"gimp-bucket-fill-tool","fill-mode","painter-compact-details"},
    {"gimp-clone-tool","align-mode","painter-compact-clone"},
    {"gimp-convolve-tool","rate","painter-compact-convolve"},
    {"gimp-dodge-burn-tool","exposure","painter-compact-details"},
    {"gimp-rotate-tool","interpolation","painter-compact-transform"},
    {"gimp-ink-tool","size-sensitivity","painter-compact-sensitivity"},
    {"gimp-text-tool","hint-style","painter-compact-details"}
  };
  for(guint n=0;n<G_N_ELEMENTS(cases);n++) {
    g_test_message("Popup contract %s / %s / %s",cases[n][0],cases[n][1],cases[n][2]);
    GimpToolInfo *tool=info(cases[n][0]);GtkWidget *gui=gimp_tools_get_tool_options_gui(tool->tool_options);
    compact(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *button=find(gui,cases[n][2],GTK_TYPE_MENU_BUTTON);g_assert_nonnull(button);
    GtkWidget *popup=GTK_WIDGET(gtk_menu_button_get_popover(GTK_MENU_BUTTON(button)));
    g_assert_nonnull(find(popup,cases[n][1],0));
    compact(gui,tool->tool_options,FALSE,GTK_ORIENTATION_HORIZONTAL);
  }
}
static void destroy_on_parent_set (GtkWidget *widget,GtkWidget *previous,gpointer owner) {
  if(previous)gtk_widget_destroy(GTK_WIDGET(owner));
}
static void reentrant_destroy_during_compaction (void) {
  GimpToolInfo *tool=info("gimp-paintbrush-tool");
  GtkWidget *gui=gtk_box_new(GTK_ORIENTATION_VERTICAL,4);g_object_ref_sink(gui);
  GtkWidget *body=gimp_prop_spin_scale_new(G_OBJECT(tool->tool_options),"jitter-amount",.1,1.,2);
  GtkWidget *toggle=NULL,*frame=gimp_prop_expanding_frame_new(G_OBJECT(tool->tool_options),"use-jitter",NULL,body,&toggle);
  gtk_box_pack_start(GTK_BOX(gui),frame,FALSE,FALSE,0);
  g_signal_connect(toggle,"parent-set",G_CALLBACK(destroy_on_parent_set),gui);
  GError *error=NULL;
  g_assert_false(gimp_painter_compact_options_set(gui,tool->tool_options,TRUE,GTK_ORIENTATION_HORIZONTAL,&error));
  g_clear_error(&error);g_object_unref(gui);
}
static void native_visibility_preference (void) {
  GimpCoreConfig *config=GIMP_CORE_CONFIG(g_object_new(GIMP_TYPE_RC,"gimp",gimp,NULL));g_assert_false(config->painter_canvas_ui);
  GError *error=NULL;gchar *converted=gimp_painter_profile_transform("# old defaults\n","gimprc",TRUE,&error);g_assert_no_error(error);
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(config),converted,-1,NULL,&error));g_assert_no_error(error);g_free(converted);g_assert_true(config->painter_canvas_ui);
  g_object_set(config,"painter-canvas-ui",FALSE,NULL);gchar *saved=gimp_config_serialize_to_string(GIMP_CONFIG(config),NULL);g_assert_nonnull(strstr(saved,"(painter-canvas-ui no)"));
  GimpCoreConfig *copy=GIMP_CORE_CONFIG(g_object_new(GIMP_TYPE_RC,"gimp",gimp,NULL));g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(copy),saved,-1,NULL,&error));g_assert_no_error(error);g_assert_false(copy->painter_canvas_ui);
  g_free(saved);g_object_unref(copy);g_object_unref(config);
}
static GimpDisplayShell *new_shell (void) {
  gimp_set_focused_once(gimp);gimp_test_utils_create_image(gimp,256,256);drain();
  GimpDisplay *display=gimp_context_get_display(gimp_get_user_context(gimp));g_assert_nonnull(display);return gimp_display_get_shell(display);
}
static void close_shell (GimpDisplayShell *shell) {
  GimpImage *image=gimp_display_get_image(shell->display);gimp_display_delete(shell->display);g_object_unref(image);drain();
}
static void canvas_dock_tool_transfer_and_preference (void) {
  gboolean old=gimp->config->painter_canvas_ui;
  g_object_set(gimp->config,"painter-canvas-ui",FALSE,NULL);
  GimpToolInfo *paint=info("gimp-painter-mypaint-tool"),*rect=info("gimp-rect-select-tool");
  gimp_context_set_tool(gimp_get_user_context(gimp),paint);
  GtkWidget *native=gimp_tools_get_tool_options_gui(paint->tool_options);
  GtkWidget *parent=gtk_widget_get_parent(native);
  GimpDisplayShell *a=new_shell();
  gimp_painter_canvas_ui_set_visible(a,TRUE);drain();
  g_assert_false(gimp_painter_compact_options_get(native));
  g_assert_true(gtk_widget_get_parent(native)==parent);
  g_assert_nonnull(find(a->canvas,"painter-compact-mypaint",GTK_TYPE_MENU_BUTTON));
  g_assert_null(find(a->canvas,"painter-canvas-undo",GTK_TYPE_BUTTON));
  gimp_context_set_tool(gimp_get_user_context(gimp),rect);drain();
  g_assert_true(gtk_widget_get_parent(native)==parent);
  g_assert_null(find(a->canvas,"painter-compact-mypaint",GTK_TYPE_MENU_BUTTON));
  gimp_context_set_tool(gimp_get_user_context(gimp),paint);drain();
  g_assert_nonnull(find(a->canvas,"painter-compact-mypaint",GTK_TYPE_MENU_BUTTON));
  for(int i=0;i<3;i++) {gimp_painter_canvas_ui_set_visible(a,FALSE);gimp_painter_canvas_ui_set_visible(a,TRUE);drain();}
  g_assert_true(gimp_tools_get_tool_options_gui(paint->tool_options)==native);
  g_assert_true(gtk_widget_get_parent(native)==parent);
  close_shell(a);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  GimpDisplayShell *c=new_shell();g_assert_true(gimp_painter_canvas_ui_get_visible(c));
  g_object_set(gimp->config,"painter-canvas-ui",FALSE,NULL);drain();
  g_assert_false(gimp_painter_canvas_ui_get_visible(c));close_shell(c);
  GimpDisplayShell *d=new_shell();g_assert_false(gimp_painter_canvas_ui_get_visible(d));close_shell(d);
  g_object_set(gimp->config,"painter-canvas-ui",old,NULL);
}
/* Original 08.017: native options boxes retain one store through presentation
 * cycles; closed registration remains closed. Insert before main(). */
static void common_store_registration_and_close (void)
{
  gimp_set_focused_once (gimp);
  GimpToolInfo *tool = info ("gimp-paintbrush-tool");
  GtkWidget *gui = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *other = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  g_object_ref_sink (gui); g_object_ref_sink (other);
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  GQuark foreign_key = g_quark_from_static_string ("painter-08017-foreign-marker");
  guint marker = 17;
  g_object_set_qdata (G_OBJECT (gui), foreign_key, &marker);
  g_assert_false (gimp_painter_compact_options_get (gui));
  g_assert_null (g_object_get_qdata (G_OBJECT (gui), key));
  GError *error = NULL;
  g_assert_true (gimp_painter_binding_close (G_OBJECT (gui), &error));
  g_assert_no_error (error);
  g_assert_null (g_object_get_qdata (G_OBJECT (gui), key));
  compact (gui, tool->tool_options, TRUE, GTK_ORIENTATION_HORIZONTAL);
  compact (other, tool->tool_options, TRUE, GTK_ORIENTATION_HORIZONTAL);
  gpointer identity = g_object_get_qdata (G_OBJECT (gui), key);
  gpointer second_identity = g_object_get_qdata (G_OBJECT (other), key);
  g_assert_nonnull (identity); g_assert_nonnull (second_identity);
  g_assert_true (identity != second_identity);
  for (guint cycle = 0; cycle < 3; ++cycle)
    {
      compact (gui, tool->tool_options, FALSE, GTK_ORIENTATION_HORIZONTAL);
      compact (gui, tool->tool_options, TRUE, GTK_ORIENTATION_VERTICAL);
      compact (gui, tool->tool_options, TRUE, GTK_ORIENTATION_HORIZONTAL);
      g_assert_true (g_object_get_qdata (G_OBJECT (gui), key) == identity);
      g_assert_true (g_object_get_qdata (G_OBJECT (gui), foreign_key) == &marker);
    }
  g_assert_true (gimp_painter_binding_close (G_OBJECT (gui), &error));
  g_assert_no_error (error);
  g_assert_false (gimp_painter_compact_options_get (gui));
  g_assert_true (gimp_painter_compact_options_get (other));
  g_assert_false (gimp_painter_compact_options_set (gui, tool->tool_options, TRUE,
                                                 GTK_ORIENTATION_HORIZONTAL, &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED);
  g_clear_error (&error);
  g_assert_true (gimp_painter_binding_close (G_OBJECT (gui), &error));
  g_assert_no_error (error);
  g_assert_true (g_object_get_qdata (G_OBJECT (gui), key) == identity);
  g_assert_true (g_object_get_qdata (G_OBJECT (other), key) == second_identity);
  g_assert_true (g_object_get_qdata (G_OBJECT (gui), foreign_key) == &marker);
  gboolean first_gone = FALSE, second_gone = FALSE;
  g_object_weak_ref (G_OBJECT (gui), finalized, &first_gone);
  g_object_weak_ref (G_OBJECT (other), finalized, &second_gone);
  gtk_widget_destroy (gui); g_object_unref (gui); g_assert_true (first_gone);
  g_assert_false (second_gone);
  compact (other, tool->tool_options, FALSE, GTK_ORIENTATION_HORIZONTAL);
  gtk_widget_destroy (other); g_object_unref (other); g_assert_true (second_gone);
}

static void widget_store_finalized (gpointer data)
{ *(gboolean *) data = TRUE; }

static void mypaint_popup_store_and_page_retention (void)
{
  gimp_set_focused_once (gimp);
  GimpToolInfo *tool = info ("gimp-painter-mypaint-tool");
  GtkWidget *host = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  GtkWidget *gui = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  g_object_ref_sink (host); g_object_ref_sink (gui);
  GError *error = NULL;
  GtkWidget *editor = gimp_painter_mybrush_editor_new (GIMP_CONTEXT (tool->tool_options), FALSE, &error);
  g_assert_no_error (error); g_assert_nonnull (editor); g_object_ref_sink (editor);
  gtk_box_pack_start (GTK_BOX (gui), editor, TRUE, TRUE, 0);
  gtk_container_add (GTK_CONTAINER (host), gui);
  gtk_widget_show_all (host);
  GQuark key = g_quark_from_static_string ("gimp-painter-binding-store-v1");
  gpointer editor_store = g_object_get_qdata (G_OBJECT (editor), key);
  g_assert_nonnull (editor_store);
  GtkWidget *notebook = find (editor, NULL, GTK_TYPE_NOTEBOOK);
  g_assert_nonnull (notebook);
  g_assert_cmpint (gtk_notebook_get_n_pages (GTK_NOTEBOOK (notebook)), >=, 3);
  gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 2);
  for (guint cycle = 0; cycle < 4; ++cycle)
    {
      compact (gui, tool->tool_options, TRUE,
               cycle % 2 ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
      GtkWidget *button = find (gui, "painter-compact-mypaint", GTK_TYPE_MENU_BUTTON);
      g_assert_nonnull (button);
      GtkWidget *popup = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (button)));
      g_assert_nonnull (popup);
      g_assert_true (find (popup, NULL, GIMP_TYPE_PAINTER_MYBRUSH_EDITOR) == editor);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE); drain ();
      g_assert_true (gtk_widget_get_visible (popup));
      g_assert_cmpint (gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook)), ==, 2);
      g_assert_true (g_object_get_qdata (G_OBJECT (editor), key) == editor_store);
      g_assert_true (g_object_get_qdata (G_OBJECT (gui), key) != editor_store);
      gimp_painter_compact_options_popdown (gui); drain ();
      compact (gui, tool->tool_options, FALSE, GTK_ORIENTATION_HORIZONTAL);
      g_assert_true (gtk_widget_get_parent (editor) == gui);
      g_assert_cmpint (gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook)), ==, 2);
    }
  gboolean editor_gone = FALSE, gui_gone = FALSE;
  g_object_set_data_full (G_OBJECT (editor), "08017-finalized", &editor_gone, widget_store_finalized);
  g_object_set_data_full (G_OBJECT (gui), "08017-finalized", &gui_gone, widget_store_finalized);
  gtk_widget_destroy (host);
  g_assert_false (gimp_painter_mybrush_editor_set_context (GIMP_PAINTER_MYBRUSH_EDITOR (editor),
                                                        GIMP_CONTEXT (tool->tool_options), &error));
  g_assert_error (error, GIMP_PAINTER_ERROR, GIMP_PAINTER_ERROR_CLOSED); g_clear_error (&error);
  g_assert_false (editor_gone); g_assert_false (gui_gone);
  g_object_unref (gui); g_assert_true (gui_gone);
  g_object_unref (editor); g_assert_true (editor_gone);
  g_object_unref (host); drain ();
}

/* Exact active source duty 01.002/001613: mixed-width native editor buttons.
 * The default constructor path remains active in both old and modern code. */
static void native_editor_mixed_width_buttons (void)
{
  gimp_set_focused_once (gimp);
  GtkWidget *host = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_object_ref_sink (host);
  GtkWidget *editor = gimp_editor_new ();
  gtk_container_add (GTK_CONTAINER (host), editor);
  GtkWidget *narrow = gimp_editor_add_button (GIMP_EDITOR (editor), "document-new",
                                             "Narrow", NULL, NULL, NULL, NULL);
  GtkWidget *wide = gimp_editor_add_button (GIMP_EDITOR (editor), "document-new",
                                           "Wide", NULL, NULL, NULL, NULL);
  gtk_widget_set_size_request (narrow, 48, -1);
  gtk_widget_set_size_request (wide, 144, -1);
  GtkBox *box = gimp_editor_get_button_box (GIMP_EDITOR (editor));
  g_assert_nonnull (box);
  g_assert_false (gtk_box_get_homogeneous (box));
  gtk_widget_show_all (host); drain ();
  for (guint rtl = 0; rtl < 2; ++rtl)
    {
      gtk_widget_set_direction (GTK_WIDGET (box), rtl ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR);
      gint width = 0, height = 0;
      gtk_widget_get_preferred_width (GTK_WIDGET (box), &width, NULL);
      gtk_widget_get_preferred_height (GTK_WIDGET (box), &height, NULL);
      GtkAllocation allocation = {0, 0, width, height};
      gtk_widget_size_allocate (GTK_WIDGET (box), &allocation);
      g_assert_cmpint (gtk_widget_get_allocated_width (narrow), <,
                       gtk_widget_get_allocated_width (wide));
      g_assert_cmpint (gtk_widget_get_allocated_width (wide), >=, 144);
      g_assert_true (gtk_widget_get_parent (narrow) == GTK_WIDGET (box));
      g_assert_true (gtk_widget_get_parent (wide) == GTK_WIDGET (box));
    }
  gimp_editor_set_box_style (GIMP_EDITOR (editor), box);
  g_assert_false (gtk_box_get_homogeneous (box));
  gtk_widget_destroy (host); g_object_unref (host); drain ();
}

#include "test-widget-store-native-controls.inc"
#include "test-widget-store-orientation.inc"
#include "test-widget-store-orientation-callers.inc"
#include "test-painter-independent-toolbar.inc"
#include "test-painter-align-toolbar.inc"
#include "test-painter-vector-toolbar.inc"
#include "test-painter-rectangle-toolbar.inc"
#include "test-painter-rectangle-unit-popup.inc"
#include "test-painter-histogram-toolbar.inc"
#include "test-painter-foreground-toolbar.inc"
#include "test-painter-text-toolbar.inc"

#include "test-painter-toolbar-lifetime.inc"

int main(int argc,char **argv) {
  g_test_init(&argc,&argv,NULL);if(!gtk_init_check(&argc,&argv))return GIMP_EXIT_TEST_SKIPPED;
  gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);
  window=gtk_window_new(GTK_WINDOW_TOPLEVEL);g_object_ref_sink(window);gtk_window_set_default_size(GTK_WINDOW(window),1100,130);
  g_test_add_func ("/painter-compact-options/rectangle-units-real-popup-events",
                   rectangle_units_real_popup_events);
  g_test_add_func ("/painter-compact-options/rectangle-units-popup-model-reentry",
                   rectangle_units_popup_model_reentry);
  g_test_add_func ("/painter-compact-options/rectangle-units-native-baseline", rectangle_units_native_baseline);
  g_test_add_func ("/painter-compact-options/rectangle-units-numeric-and-sync", rectangle_units_numeric_and_sync);
  g_test_add_func ("/painter-compact-options/rectangle-units-metadata-and-lifetimes", rectangle_units_metadata_and_lifetimes);
  g_test_add_func ("/painter-compact-options/rectangle-units-options-lifetime-and-reentry", rectangle_units_options_lifetime_and_reentry);
  g_test_add_func ("/painter-compact-options/rectangle-units-weak-helper-lifetime", rectangle_units_weak_helper_lifetime);
  g_test_add_func ("/painter-compact-options/rectangle-units-native-canvas-baseline", rectangle_units_native_canvas_baseline);
  g_test_add_func ("/painter-compact-options/independent-rectangle-toolbar", independent_rectangle_toolbar);
  g_test_add_func ("/painter-compact-options/rectangle-native-shrink-bounds", rectangle_native_shrink_bounds);
  g_test_add_func ("/painter-compact-options/rectangle-shrink-lifecycle", rectangle_shrink_lifecycle);
  g_test_add_func ("/painter-compact-options/rectangle-shrink-owner-lifetime-and-reentry", rectangle_shrink_owner_lifetime_and_reentry);
  g_test_add_func ("/painter-compact-options/independent-foreground-toolbar", independent_foreground_toolbar);
  g_test_add_func ("/painter-compact-options/independent-text-toolbar", independent_text_toolbar);
  g_test_add_func ("/painter-compact-options/text-toolbar-model-before-view", text_toolbar_model_before_view);
  g_test_add_func ("/painter-compact-options/text-toolbar-units-images", text_toolbar_units_images);
  g_test_add_func ("/painter-compact-options/text-toolbar-native-units", text_toolbar_native_units);
  g_test_add_func ("/painter-compact-options/text-toolbar-canvas-popup-lifetime", text_toolbar_canvas_popup_lifetime);
  g_test_add_func ("/painter-compact-options/foreground-toolbar-model-before-view", foreground_toolbar_model_before_view);
  g_test_add_func ("/painter-compact-options/foreground-toolbar-canvas-popup-lifetime", foreground_toolbar_canvas_popup_lifetime);
  g_test_add_func ("/painter-compact-options/independent-histogram-toolbar", independent_histogram_toolbar);
  g_test_add_func ("/painter-compact-options/histogram-toolbar-native-dialogs", histogram_toolbar_native_dialogs);
  g_test_add_func ("/painter-compact-options/histogram-toolbar-canvas-popup-lifetime", histogram_toolbar_canvas_popup_lifetime);
  g_test_add_func ("/painter-compact-options/independent-vector-toolbar", independent_vector_toolbar);
  g_test_add_func ("/painter-compact-options/vector-selection-actions", vector_selection_actions);
  g_test_add_func ("/painter-compact-options/vector-native-dialog-actions", vector_native_dialog_actions);
  g_test_add_func ("/painter-compact-options/vector-model-before-view", vector_model_before_view);
  g_test_add_func ("/painter-compact-options/vector-path-lifecycle-and-reentry", vector_path_lifecycle_and_reentry);
  g_test_add_func ("/painter-compact-options/independent-align-toolbar", independent_align_toolbar);
  g_test_add_func ("/painter-compact-options/align-model-before-view", align_model_before_view);
  g_test_add_func ("/painter-compact-options/align-actions-and-reentry", align_actions_and_reentry);
  g_test_add_func ("/painter-compact-options/independent-mypaint-toolbar", independent_mypaint_toolbar);
  g_test_add_func ("/painter-compact-options/toolbar-lifetime-observer-control", toolbar_lifetime_observer_control);
  g_test_add_func ("/painter-compact-options/canvas-toolbar-ref-balance", toolbar_canvas_ref_balance);
  g_test_add_func ("/painter-compact-options/independent-perspective-guide-toolbar", independent_perspective_guide_toolbar);
  g_test_add_func ("/painter-compact-options/perspective-guide-toolbar-canvas-switching", perspective_guide_toolbar_canvas_switching);
  g_test_add_func ("/painter-compact-options/independent-horizontal-inventory", independent_horizontal_toolbar_inventory);
  g_test_add_func ("/painter-compact-options/independent-horizontal-binding", independent_horizontal_toolbar_numeric_binding);
  g_test_add_func ("/painter-compact-options/independent-horizontal-native-handles", independent_horizontal_toolbar_native_handles);
  g_test_add_func ("/painter-compact-options/orientation-enum-contract", orientation_enum_contract);
  g_test_add_func ("/painter-compact-options/orientation-integer-contract", orientation_integer_contract);
  g_test_add_func ("/painter-compact-options/orientation-property-contract", orientation_property_contract);
  g_test_add_func ("/painter-compact-options/orientation-active-callers", orientation_active_callers);
  g_test_add_func ("/painter-compact-options/orientation-histogram-current-controls", orientation_histogram_current_controls);
  g_test_add_func ("/painter-compact-options/native-preview-factory-failure", native_preview_factory_failure);
  g_test_add_func ("/painter-compact-options/native-preview-factory-success", native_preview_factory_success);
  g_test_add_func ("/painter-compact-options/native-property-controls", native_property_controls);
  g_test_add_func ("/painter-compact-options/native-enum-integer-helpers", native_enum_integer_helpers);
  g_test_add_func("/painter-compact-options/inventory-all-registered",inventory_all_registered_tools);
  g_test_add_func("/painter-compact-options/common-store-registration-close",common_store_registration_and_close);
  g_test_add_func("/painter-compact-options/mypaint-store-page-retention",mypaint_popup_store_and_page_retention);
  g_test_add_func("/painter-compact-options/native-editor-mixed-width",native_editor_mixed_width_buttons);
  g_test_add_func("/painter-compact-options/paint-bidirectional-resource",paint_bidirectional_reset_and_resource);
  g_test_add_func("/painter-compact-options/popup-repeat",popup_repeat);
  g_test_add_func("/painter-compact-options/popup-native-events-window-destroy",popup_native_events_and_window_destroy);
  g_test_add_func("/painter-compact-options/fresh-owner-lifecycle",fresh_owner_lifecycle);
  g_test_add_func("/painter-compact-options/mixed-packing-owned-destruction",mixed_packing_and_owned_destruction);
  g_test_add_func("/painter-compact-options/native-viewport-composition",native_viewport_composition);
  g_test_add_func("/painter-compact-options/registered-popup-contracts",registered_popup_contracts);
  g_test_add_func("/painter-compact-options/reentrant-destruction",reentrant_destroy_during_compaction);
  g_test_add_func("/painter-compact-options/native-visibility-preference",native_visibility_preference);
  g_test_add_func("/painter-compact-options/canvas-dock-tool-transfer-preference",canvas_dock_tool_transfer_and_preference);
  g_application_run(gimp->app,0,NULL);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  gtk_widget_destroy(window);g_object_unref(window);g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
