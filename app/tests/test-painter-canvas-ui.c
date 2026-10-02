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
#include "gimp-app-test-utils.h"
#include "tests.h"
#include "gimpcoreapp.h"
static Gimp *gimp;
static GtkWidget *window;
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
#define ADD(f) g_test_add_func("/painter-canvas-ui/" #f,f)
  if(g_getenv("PAINTER_CANVAS_DEMO")) { ADD(demo); } else {
  ADD(channel_preview_components);ADD(selection_and_visibility);ADD(group_move_and_undo);ADD(image_switch_disconnect);ADD(long_press_cancel_destroy);ADD(preview_pending_destroy);ADD(popup_repeat_and_owner_close);
  ADD(popup_controls_and_clone);ADD(mask_controls);ADD(long_press_once);ADD(reentrant_visibility_close);ADD(multiple_reorder_one_undo);ADD(hide_cancels_grab);
  ADD(overlay_input_and_restore);ADD(dock_restore_and_parent_close);ADD(overlay_tools_and_colors);ADD(overlay_tool_group);ADD(multiple_canvas_context_and_dock_transfer);ADD(overlay_owner_destroy_callbacks);
  }
  g_application_run(gimp->app,0,NULL);
  int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));
  gtk_widget_destroy(window);g_object_unref(window);
  g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;
}
