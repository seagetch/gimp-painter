/* Original25.009 checkerboard: independent viewport transform, real window. */
#include "core/gimp-cairo.h"
#include "libgimpcolor/gimpcolor.h"
#include "widgets/gimprender.h"
static void checker_window_geometry(gconstpointer data)
{
  guint a=GPOINTER_TO_UINT(data);gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  /* Hide the sole layer, preserving its pixel checksum; transparency is real. */
  gimp_item_set_visible(GIMP_ITEM(scene.drawable),FALSE,FALSE);gimp_image_flush(gimp_display_get_image(shell->display));
  gimp_display_shell_scale(shell,GIMP_ZOOM_TO,4.0,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,512-shell->disp_width/2,512-shell->disp_height/2);
      DragPoint start={shell->disp_width/2+40.25,shell->disp_height/2+60.25};
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});assert_navigation_idle();
      LegacyAppliedZoom view=old_zoom_view();
      cairo_surface_t*expected=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,shell->disp_width,shell->disp_height);
      cairo_t*cr=grid_geometry_old_ctm(expected,&view);
      GimpCheckSize size;g_object_get(shell->display->config,"transparency-size",&size,NULL);
      cairo_pattern_t*pattern=gimp_cairo_checkerboard_create(cr,1<<(size+2),gimp_render_check_color1(),gimp_render_check_color2());
      cairo_translate(cr,-shell->offset_x,-shell->offset_y);cairo_set_source(cr,pattern);cairo_paint(cr);
      cairo_destroy(cr);cairo_pattern_destroy(pattern);cairo_surface_flush(expected);
      gimp_display_shell_render_invalidate_full(shell);gtk_widget_queue_draw(shell->canvas);GdkPixbuf*frame=grid_window_capture();
      guint pixels=0;gint channels=gdk_pixbuf_get_n_channels(frame);
      for(gint y=shell->disp_height/2-235;y<shell->disp_height/2+235;y++)for(gint x=shell->disp_width/2-235;x<shell->disp_width/2+235;x++)
        {
          guint32 q=draw_pixel(expected,x,y);guchar*p=gdk_pixbuf_get_pixels(frame)+y*gdk_pixbuf_get_rowstride(frame)+x*channels;
          for(guint c=0;c<3;c++)if(ABS(p[c]-(gint)((q>>(16-8*c))&255))>2)
            g_error("checker angle=%g H=%u V=%u pixel=%d,%d channel=%u actual=%u expected=%u",drag_angles[a].angle,h,v,x,y,c,p[c],(q>>(16-8*c))&255);
          pixels++;
        }
      g_test_message("checker angle=%g H=%u V=%u pixels=%u",drag_angles[a].angle,h,v,pixels);
      if(a==1&&h&&v)grid_damage_screenshot("checker","shown");
      g_object_unref(frame);cairo_surface_destroy(expected);
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
