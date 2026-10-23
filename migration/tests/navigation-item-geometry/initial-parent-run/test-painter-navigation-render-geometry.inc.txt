/* Original25.009: image render chunk interiors and the common precise CTM.
 * No old renderer/runtime equivalence is asserted by this native fixture. */
#include "display/gimpdisplayshell-render.h"
static void render_chunk_interiors(gconstpointer data)
{
  guint scene_id=GPOINTER_TO_UINT(data),a=scene_id%3;gboolean aniso=scene_id>=3,previous_ui;guint scenes=0;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);gimp_test_run_temp_mainloop(120);
  if(aniso){gimp_image_set_resolution(gimp_display_get_image(shell->display),144,72);gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);}
  gimp_display_shell_scale(shell,GIMP_ZOOM_TO,4.0,GIMP_ZOOM_FOCUS_IMAGE_CENTER);
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,RINT(128*shell->scale_x)-shell->disp_width/2,RINT(128*shell->scale_y)-shell->disp_height/2);
      DragPoint start={shell->disp_width/2+40.25,shell->disp_height/2+60.25};
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});assert_navigation_idle();
      g_assert_cmpfloat(fabs(shell->painter_pan_fraction_x)+fabs(shell->painter_pan_fraction_y),>,0.0);
      LegacyAppliedZoom view=old_zoom_view();LegacyPanShell old={0};old.disp_width=view.disp_width;old.disp_height=view.disp_height;old.rotate_angle=view.rotate_angle;old.mirrored=view.mirrored;
      cairo_t*inverse=legacy_pan_cairo();legacy_pan_set_rotate(&old,inverse);
      /* Re-render every visible chunk, not a previously valid render-cache copy. */
      gimp_display_shell_render_invalidate_full(shell);gtk_widget_queue_draw(shell->canvas);GdkPixbuf*frame=grid_window_capture();
      gint width=gdk_pixbuf_get_width(frame),height=gdk_pixbuf_get_height(frame),channels=gdk_pixbuf_get_n_channels(frame);guint pixels=0;
      g_assert_cmpint(shell->render_buf_width,<=,256);g_assert_cmpint(shell->render_buf_height,<=,256);
      /* The470px ROI spans multiple native256px-or-smaller screen chunks. */
      for(gint y=height/2-235;y<height/2+235;y++)for(gint x=width/2-235;x<width/2+235;x++)
        {
          gdouble ix=x+0.5,iy=y+0.5;cairo_device_to_user(inverse,&ix,&iy);ix=(ix+view.offset_x)/view.scale_x;iy=(iy+view.offset_y)/view.scale_y;
          if(ix<4||iy<4||ix>252||iy>252)continue;
          gboolean landmark=FALSE;for(guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)
            if(ix>draw_landmarks[n].x-4&&ix<draw_landmarks[n].x+draw_landmarks[n].w+4&&iy>draw_landmarks[n].y-4&&iy<draw_landmarks[n].y+draw_landmarks[n].h+4)landmark=TRUE;
          if(landmark)continue;
          guchar*p=gdk_pixbuf_get_pixels(frame)+y*gdk_pixbuf_get_rowstride(frame)+x*channels;
          if(ABS(p[0]-128)>2||ABS(p[1]-128)>2||ABS(p[2]-128)>2)
            g_error("chunk interior angle=%g H=%u V=%u aniso=%u pixel=%d,%d image=%.9g,%.9g RGB=%u,%u,%u",drag_angles[a].angle,h,v,aniso,x,y,ix,iy,p[0],p[1],p[2]);
          pixels++;
        }
      g_assert_cmpuint(pixels,>,20000);g_test_message("chunk interiors angle=%g H=%u V=%u aniso=%u pixels=%u fractional=%.9g,%.9g",drag_angles[a].angle,h,v,aniso,pixels,shell->painter_pan_fraction_x,shell->painter_pan_fraction_y);
      if(a==1&&aniso&&h&&v)grid_damage_screenshot("chunk-render","shown");
      cairo_destroy(inverse);g_object_unref(frame);scenes++;
    }
  g_assert_cmpuint(scenes,==,4);if(aniso)gimp_display_shell_scale_set_dot_for_dot(shell,TRUE);
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

/* Native image-edge clipping is independent of retired TileManager internals. */
static void render_image_edges(gconstpointer data)
{
  guint a=GPOINTER_TO_UINT(data);gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  const gdouble factors[]={0.5,1.0,4.0};
  for(guint z=0;z<3;z++)for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_scale(shell,GIMP_ZOOM_TO,factors[z],GIMP_ZOOM_FOCUS_IMAGE_CENTER);
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,-shell->disp_width/2,-shell->disp_height/2);
      DragPoint start={shell->disp_width/2+40.25,shell->disp_height/2+60.25};
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();
      drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});
      drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});assert_navigation_idle();
      gimp_display_shell_set_show_image(shell,FALSE);GdkPixbuf*base=grid_window_capture();
      gimp_display_shell_set_show_image(shell,TRUE);gimp_display_shell_render_invalidate_full(shell);gtk_widget_queue_draw(shell->canvas);GdkPixbuf*frame=grid_window_capture();
      LegacyAppliedZoom view=old_zoom_view();LegacyPanShell old={0};old.disp_width=view.disp_width;old.disp_height=view.disp_height;old.rotate_angle=view.rotate_angle;old.mirrored=view.mirrored;
      cairo_t*inverse=legacy_pan_cairo();legacy_pan_set_rotate(&old,inverse);guint inside=0,outside=0;
      g_assert_cmpint(gdk_pixbuf_get_width(base),==,gdk_pixbuf_get_width(frame));g_assert_cmpint(gdk_pixbuf_get_height(base),==,gdk_pixbuf_get_height(frame));
      gint channels=gdk_pixbuf_get_n_channels(frame);
      for(gint y=shell->disp_height/2-235;y<shell->disp_height/2+235;y++)for(gint x=shell->disp_width/2-235;x<shell->disp_width/2+235;x++)
        {
          gdouble ix=x+0.5,iy=y+0.5;cairo_device_to_user(inverse,&ix,&iy);ix=(ix+view.offset_x)/view.scale_x;iy=(iy+view.offset_y)/view.scale_y;
          gboolean out=ix < -4||iy < -4||ix > 260||iy > 260;
          gboolean in=ix>4&&iy>4&&ix<252&&iy<252;
          for(guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)if(ix>draw_landmarks[n].x-4&&ix<draw_landmarks[n].x+draw_landmarks[n].w+4&&iy>draw_landmarks[n].y-4&&iy<draw_landmarks[n].y+draw_landmarks[n].h+4)in=FALSE;
          if(!in&&!out)continue;
          guchar*p=gdk_pixbuf_get_pixels(frame)+y*gdk_pixbuf_get_rowstride(frame)+x*channels;
          guchar*b=gdk_pixbuf_get_pixels(base)+y*gdk_pixbuf_get_rowstride(base)+x*channels;
          for(guint c=0;c<3;c++)if(ABS(p[c]-(in?128:b[c]))>2)g_error("image edge angle=%g zoom=%g H=%u V=%u inside=%u pixel=%d,%d image=%g,%g channel=%u actual=%u expected=%u",drag_angles[a].angle,factors[z],h,v,in,x,y,ix,iy,c,p[c],in?128:b[c]);
          if(in)inside++;else outside++;
        }
      g_assert_cmpuint(inside,>,500);g_assert_cmpuint(outside,>,500);
      g_test_message("image edges angle=%g zoom=%g H=%u V=%u inside=%u outside=%u",drag_angles[a].angle,factors[z],h,v,inside,outside);
      cairo_destroy(inverse);g_object_unref(base);g_object_unref(frame);
    }
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
