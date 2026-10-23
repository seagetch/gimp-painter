/* Native path miter damage regression; draw geometry remains unchanged. */
#include "core/gimpbezierdesc.h"
#include "display/gimpcanvaspath.h"
static GimpCanvasItem *path_damage_new(guint style,guint shape,gboolean filled)
{
  cairo_surface_t*s=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,1,1);cairo_t*cr=cairo_create(s);
  if(shape==0){cairo_move_to(cr,48,75);cairo_curve_to(cr,90,10,190,245,208,72);cairo_line_to(cr,140,205);cairo_close_path(cr);}
  else if(shape==1){cairo_move_to(cr,48,190);cairo_line_to(cr,128,45);cairo_line_to(cr,136,185);cairo_close_path(cr);}
  else {cairo_move_to(cr,45,80);cairo_line_to(cr,205,128);cairo_line_to(cr,45,135);}
  cairo_path_t*p=cairo_copy_path(cr);GimpBezierDesc*b=gimp_bezier_desc_new(g_memdup2(p->data,p->num_data*sizeof(cairo_path_data_t)),p->num_data);
  GimpCanvasItem*item=gimp_canvas_path_new(shell,b,0,0,filled,(GimpPathStyle)style);
  gimp_bezier_desc_free(b);cairo_path_destroy(p);cairo_destroy(cr);cairo_surface_destroy(s);return item;
}
static void path_damage_bounds(void)
{
  DrawScene scene=draw_scene_new();guint scenes=0;
  for(guint a=0;a<3;a++)for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);LegacyAppliedZoom view=old_zoom_view();
      for(guint style=0;style<3;style++)for(guint shape=0;shape<3;shape++)for(guint filled=0;filled<2;filled++)
        {
          GimpCanvasItem*item=path_damage_new(style,shape,filled);
          cairo_surface_t*s=cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA,NULL);cairo_t*cr=cairo_create(s);
          if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);gimp_canvas_item_draw(item,cr);cairo_destroy(cr);
          gdouble x,y,w,hh;cairo_recording_surface_ink_extents(s,&x,&y,&w,&hh);
          cairo_region_t*r=grid_geometry_damage(item,&view);cairo_rectangle_int_t b;cairo_region_get_extents(r,&b);
          if(x<b.x||y<b.y||x+w>b.x+b.width||y+hh>b.y+b.height)
            g_error("path bounds angle=%g H=%u V=%u style=%u shape=%u filled=%u ink=%g,%g %gx%g damage=%d,%d %dx%d",drag_angles[a].angle,h,v,style,shape,filled,x,y,w,hh,b.x,b.y,b.width,b.height);
          g_assert_cmpfloat(w,>,0);g_assert_cmpfloat(hh,>,0);scenes++;cairo_region_destroy(r);cairo_surface_destroy(s);g_object_unref(item);
        }
    }
  g_assert_cmpuint(scenes,==,216);g_test_message("path recording bounds scenes=%u",scenes);draw_scene_free(&scene);
}
/* Wait for the initial GTK/oncanvas allocation, never resize or repair a
 * capture between show and hide. A later geometry change still fails hard. */
static void path_damage_wait_layout(void)
{
  gint w=-1,h=-1;guint stable=0;
  for(guint n=0;n<30&&stable<5;n++)
    {
      gimp_test_run_temp_mainloop(100);
      gint nw=gtk_widget_get_allocated_width(shell->canvas),nh=gtk_widget_get_allocated_height(shell->canvas);
      stable=(nw==w&&nh==h&&nw==shell->disp_width&&nh==shell->disp_height)?stable+1:0;
      w=nw;h=nh;
    }
  g_assert_cmpuint(stable,==,5);
}
static void path_damage_window(gconstpointer data)
{
  guint id=GPOINTER_TO_UINT(data),a=id/3,style=id%3,scenes=0;gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
      for(guint shape=0;shape<3;shape++)for(guint filled=0;filled<2;filled++)
        {
          GimpCanvasItem*item=path_damage_new(style,shape,filled);gimp_canvas_item_set_visible(item,FALSE);gimp_display_shell_add_item(shell,item);
          GdkPixbuf*base=grid_window_capture();gimp_canvas_item_set_visible(item,TRUE);gtk_widget_queue_draw(shell->canvas);GdkPixbuf*shown=grid_window_capture();
          guint visible=grid_window_difference(base,shown);g_assert_cmpuint(visible,>,0);
          if(a==1&&style==1&&!shape&&!filled&&!h&&!v)grid_damage_screenshot("path","shown");
          gimp_canvas_item_set_visible(item,FALSE);GdkPixbuf*hidden=grid_window_capture();guint residual=grid_window_difference(base,hidden);
          if(a==1&&style==1&&!shape&&!filled&&!h&&!v)grid_damage_screenshot("path","hidden");
          g_test_message("path window angle=%g H=%u V=%u style=%u shape=%u filled=%u visible=%u residual=%u",drag_angles[a].angle,h,v,style,shape,filled,visible,residual);
          g_assert_cmpuint(residual,==,0);scenes++;g_object_unref(base);g_object_unref(shown);g_object_unref(hidden);gimp_display_shell_remove_item(shell,item);g_object_unref(item);
        }
    }
  g_assert_cmpuint(scenes,==,24);draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
