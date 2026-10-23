/* CENTER is an enum/property default without a corner geometry contract.
 * These tests require deterministic no ink/extents and preserved directional
 * drawing, using the already source-bound generic grid projection helpers. */
#include "display/gimpcanvascorner.h"
static GimpCanvasItem *corner_new(guint anchor,gboolean outside)
{
  return gimp_canvas_corner_new(shell,48,58,160,140,(GimpHandleAnchor)anchor,23,19,outside);
}
static void corner_default_no_ink(void)
{
  gboolean previous_ui;g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  for(guint kind=0;kind<3;kind++)
    {
      GimpCanvasItem*item=kind?corner_new(GIMP_HANDLE_ANCHOR_CENTER,kind==2):g_object_new(GIMP_TYPE_CANVAS_CORNER,"shell",shell,NULL);
      g_assert_null(gimp_canvas_item_get_extents(item));
      cairo_surface_t*surface=cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL);cairo_t*cr=cairo_create(surface);
      gimp_canvas_item_draw(item,cr);g_assert_cmpint(cairo_status(cr),==,CAIRO_STATUS_SUCCESS);cairo_destroy(cr);
      gdouble x,y,w,h;cairo_recording_surface_ink_extents(surface,&x,&y,&w,&h);g_assert_cmpfloat(w,==,0);g_assert_cmpfloat(h,==,0);
      cairo_surface_destroy(surface);g_object_unref(item);
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
static void corner_directional_geometry(void)
{
  gboolean previous_ui;g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);guint scenes=0;
  for(guint a=0;a<3;a++)for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      LegacyAppliedZoom view=old_zoom_view();
      for(guint outside=0;outside<2;outside++)for(guint anchor=1;anchor<9;anchor++)
        {
          GimpCanvasItem*item=corner_new(anchor,outside);cairo_surface_t*surface=cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL);cairo_t*cr=cairo_create(surface);
          if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);gimp_canvas_item_draw(item,cr);cairo_destroy(cr);
          gdouble x,y,w,hh;cairo_recording_surface_ink_extents(surface,&x,&y,&w,&hh);
          cairo_region_t*region=grid_geometry_damage(item,&view);cairo_rectangle_int_t r;cairo_region_get_extents(region,&r);
          g_assert_cmpfloat(w,>,0);g_assert_cmpfloat(hh,>,0);g_assert_cmpfloat(x,>=,r.x);g_assert_cmpfloat(y,>=,r.y);g_assert_cmpfloat(x+w,<=,r.x+r.width);g_assert_cmpfloat(y+hh,<=,r.y+r.height);
          cairo_region_destroy(region);cairo_surface_destroy(surface);g_object_unref(item);scenes++;
        }
    }
  g_assert_cmpuint(scenes,==,192);g_test_message("directional corner bounds scenes=%u",scenes);
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
static void corner_real_transition(gconstpointer data)
{
  guint a=GPOINTER_TO_UINT(data);gboolean previous_ui;g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);gimp_test_run_temp_mainloop(120);
  gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      for(guint outside=0;outside<2;outside++)for(guint anchor=1;anchor<9;anchor++)
        {
          GimpCanvasItem*item=corner_new(GIMP_HANDLE_ANCHOR_CENTER,outside);gimp_display_shell_add_item(shell,item);GdkPixbuf*base=grid_window_capture();
          gimp_canvas_item_begin_change(item);g_object_set(item,"anchor",anchor,NULL);gimp_canvas_item_end_change(item);
          gtk_widget_queue_draw(shell->canvas);GdkPixbuf*shown=grid_window_capture();guint visible=grid_window_difference(base,shown);g_assert_cmpuint(visible,>,0);
          if(a==1&&!h&&!v&&!outside&&anchor==GIMP_HANDLE_ANCHOR_NORTH_WEST)grid_damage_screenshot("corner","shown");
          gimp_canvas_item_begin_change(item);g_object_set(item,"anchor",GIMP_HANDLE_ANCHOR_CENTER,NULL);gimp_canvas_item_end_change(item);GdkPixbuf*hidden=grid_window_capture();
          guint residual=grid_window_difference(base,hidden);g_test_message("real corner transition angle=%g H=%u V=%u outside=%u anchor=%u visible=%u residual=%u",drag_angles[a].angle,h,v,outside,anchor,visible,residual);g_assert_cmpuint(residual,==,0);
          g_assert_null(gimp_canvas_item_get_extents(item));
          if(a==1&&!h&&!v&&!outside&&anchor==GIMP_HANDLE_ANCHOR_NORTH_WEST)grid_damage_screenshot("corner","hidden");
          g_object_unref(base);g_object_unref(shown);g_object_unref(hidden);gimp_display_shell_remove_item(shell,item);g_object_unref(item);
        }
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
