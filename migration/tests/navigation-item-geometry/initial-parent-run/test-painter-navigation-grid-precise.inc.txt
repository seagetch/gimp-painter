/* Original rotated grid clipping with fractional origin and anisotropic view. */
static void grid_precise_window(gconstpointer data)
{
  guint a=GPOINTER_TO_UINT(data),scenes=0;gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();gimp_painter_canvas_ui_set_visible(shell,TRUE);
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  gimp_image_set_resolution(gimp_display_get_image(shell->display),144,72);gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_set_show_all(shell,v);gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      gimp_display_shell_scroll_set_offset(shell,RINT(128*shell->scale_x)-shell->disp_width/2,RINT(128*shell->scale_y)-shell->disp_height/2);
      DragPoint start={shell->disp_width/2+40.25,shell->disp_height/2+60.25};
      drag_send_pointer(GDK_BUTTON_PRESS,2,0,start);assert_grab();drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,(DragPoint){start.x+2.6,start.y-1.4});assert_navigation_idle();
      for(guint style=0;style<2;style++)for(guint type=0;type<5;type++)
        {
          GimpGrid*grid=g_object_new(GIMP_TYPE_GRID,"xspacing",31.0,"yspacing",23.0,"xoffset",7.0,"yoffset",11.0,"style",type,NULL);
          GimpCanvasItem*item=gimp_canvas_grid_new(shell,grid);g_object_unref(grid);g_object_set(item,"grid-style",style,NULL);
          gimp_canvas_item_set_visible(item,FALSE);gimp_display_shell_add_item(shell,item);GdkPixbuf*base=grid_window_capture();
          gimp_canvas_item_set_visible(item,TRUE);gtk_widget_queue_draw(shell->canvas);GdkPixbuf*shown=grid_window_capture();guint visible=grid_window_difference(base,shown);g_assert_cmpuint(visible,>,0);
          gimp_canvas_item_set_visible(item,FALSE);GdkPixbuf*hidden=grid_window_capture();guint residual=grid_window_difference(base,hidden);
          g_test_message("precise grid angle=%g H=%u V=%u show-all=%u style=%u type=%u visible=%u residual=%u",drag_angles[a].angle,h,v,v,style,type,visible,residual);g_assert_cmpuint(residual,==,0);scenes++;
          g_object_unref(base);g_object_unref(shown);g_object_unref(hidden);gimp_display_shell_remove_item(shell,item);g_object_unref(item);
        }
    }
  g_assert_cmpuint(scenes,==,40);gimp_display_shell_set_show_all(shell,FALSE);gimp_display_shell_scale_set_dot_for_dot(shell,TRUE);draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}
