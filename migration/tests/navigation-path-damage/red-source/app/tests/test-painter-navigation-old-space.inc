/* Space PAN is an old configured action, not a custom mouse-button binding.
 * Its original coordinate boundary is inverse-before-gint, independent of
 * modifiers held when Space starts or while it remains down. */
#include "widgets/gimpdevices.h"
#include "widgets/gimpdevicemanager.h"
#include "widgets/gimpdeviceinfo.h"
#include "widgets/gimpdeviceinfo-coords.h"
#include "../../migration/fixtures/legacy-navigation-space/oracle.inc"
static void old_space_pan_origin(void)
{
  const guint states[]={0,GDK_SHIFT_MASK,GDK_CONTROL_MASK,GDK_SHIFT_MASK|GDK_CONTROL_MASK,GDK_MOD1_MASK};
  for(guint angle=0;angle<G_N_ELEMENTS(drag_angles);angle++)
  for(guint mirror=0;mirror<2;mirror++)
  for(guint vertical=0;vertical<2;vertical++)
  for(guint aniso=0;aniso<2;aniso++)
    {
      DragScene scene=drag_scene_new(&drag_angles[angle],mirror,1.5,TRUE,real_tool_names[0]);
      gint old_space;
      if(vertical){gimp_display_shell_flip(shell,mirror,TRUE);gimp_display_shell_rotate_to(shell,scene.angle->angle);}
      if(aniso){gimp_image_set_resolution(gimp_display_get_image(shell->display),144,72);gimp_display_shell_scale_set_dot_for_dot(shell,FALSE);}
      g_object_get(shell->display->config,"space-bar-action",&old_space,NULL);
      g_object_set(shell->display->config,"space-bar-action",GIMP_SPACE_BAR_ACTION_PAN,NULL);
      for(guint entry=0;entry<G_N_ELEMENTS(states);entry++)
        {
          LegacyAppliedZoom old=old_zoom_view();LegacyPanShell pan={0};
          GimpDeviceInfo*info=gimp_device_manager_get_current_device(gimp_devices_get_manager(gimp));
          GimpCoords coords;
          gimp_device_info_get_device_coords(info,gtk_widget_get_window(shell->canvas),&coords);
          pan.disp_width=old.disp_width;pan.disp_height=old.disp_height;
          pan.image_width=old.image_width;pan.image_height=old.image_height;
          pan.rotate_angle=old.rotate_angle;pan.mirrored=old.mirrored;
          pan.scale_x=old.scale_x;pan.scale_y=old.scale_y;pan.offset_x=old.offset_x;pan.offset_y=old.offset_y;
          legacy_space_pan_begin(&pan,NULL,&coords);
          send_event(GDK_KEY_PRESS,0,states[entry],0,0,GDK_KEY_space);assert_grab();
          /* Repeated Space must not replace the latched original anchor. */
          send_event(GDK_KEY_PRESS,0,states[entry],0,0,GDK_KEY_space);
          for(guint step=0;step<G_N_ELEMENTS(states);step++)
            {
              DragPoint motion={coords.x+17.6+step*3.3,coords.y-10.6+step*2.1};
              send_event(GDK_KEY_PRESS,0,states[step],0,0,GDK_KEY_Shift_L);
              send_event(GDK_KEY_RELEASE,0,states[step],0,0,GDK_KEY_Shift_L);
              assert_grab();
              legacy_pan_motion(&pan,motion.x,motion.y);
              drag_send_pointer(GDK_MOTION_NOTIFY,0,states[step],motion);
              old.offset_x=pan.offset_x;old.offset_y=pan.offset_y;
              old_zoom_assert(&old,2000+angle*4+mirror*2+vertical,entry,step);
              g_assert_cmpfloat_with_epsilon(shell->painter_pan_start_x,pan.scroll_start_x,1e-10);
              g_assert_cmpfloat_with_epsilon(shell->painter_pan_start_y,pan.scroll_start_y,1e-10);
            }
          send_event(GDK_KEY_RELEASE,0,0,0,0,GDK_KEY_space);assert_navigation_idle();
          g_assert_false(shell->space_release_pending);
        }
      g_object_set(shell->display->config,"space-bar-action",old_space,NULL);
      if(aniso)gimp_display_shell_scale_set_dot_for_dot(shell,TRUE);
      if(vertical)gimp_display_shell_flip(shell,mirror,FALSE);
      gimp_display_shell_rotate_to(shell,scene.angle->angle);
      drag_scene_free(&scene);
    }
}

static void old_space_pan_render (void)
{
  gboolean previous_ui;
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);
  g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  for (guint mirror=0;mirror<2;mirror++)
    {
      DrawScene scene=draw_scene_new();
      LegacyPanShell old={0};
      DragPoint start,motion;
      gint old_space;GimpCoords coords;
      gdouble origin_x,origin_y,sx,sy;
      GtkWidget *widget=gtk_event_box_new();
      GimpOverlayChild *child;
      gimp_painter_canvas_ui_set_visible(shell,TRUE);
      g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
      gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);
      gimp_test_run_temp_mainloop(100);
      gimp_display_shell_flip(shell,mirror,FALSE);
      gimp_display_shell_rotate_to(shell,37.0);
      gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
      old.disp_width=shell->disp_width;old.disp_height=shell->disp_height;
      old.image_width=old.image_height=256;old.scale_x=old.scale_y=1.0;
      old.rotate_angle=mirror?-37.0:37.0;old.mirrored=mirror;
      gimp_display_shell_transform_xy_f(shell,0,0,&origin_x,&origin_y);
      legacy_pan_inverse(&old,&origin_x,&origin_y);old.offset_x=-origin_x;old.offset_y=-origin_y;
      g_object_get(shell->display->config,"space-bar-action",&old_space,NULL);
      g_object_set(shell->display->config,"space-bar-action",GIMP_SPACE_BAR_ACTION_PAN,NULL);
      gimp_device_info_get_device_coords(gimp_device_manager_get_current_device(gimp_devices_get_manager(gimp)),gtk_widget_get_window(shell->canvas),&coords);
      start=(DragPoint){coords.x,coords.y};legacy_space_pan_begin(&old,NULL,&coords);
      gtk_widget_set_size_request(widget,19,11);
      gimp_display_shell_add_overlay(shell,widget,40,220,GIMP_HANDLE_ANCHOR_NORTH_WEST,0,0);
      gtk_widget_show(widget);
      child=gimp_overlay_child_find(GIMP_OVERLAY_BOX(shell->canvas),widget);
      g_assert_nonnull(child);
      send_event(GDK_KEY_PRESS,0,GDK_SHIFT_MASK|GDK_CONTROL_MASK,0,0,GDK_KEY_space);assert_grab();
      for (guint step=0;step<3;step++)
        {
          cairo_surface_t *surface;
          gchar *name=g_strdup_printf("legacy-space-oncanvas-m%u-step%u",mirror,step);
          motion=(DragPoint){start.x+17.6+step*0.13,start.y-10.6-step*0.17};
          legacy_pan_motion(&old,motion.x,motion.y);
          drag_send_pointer(GDK_MOTION_NOTIFY,0,step%2?GDK_SHIFT_MASK:GDK_CONTROL_MASK,motion);
          surface=draw_snapshot(&scene,name);g_free(name);
          for (guint n=0;n<G_N_ELEMENTS(draw_landmarks);n++)
            {
              const DrawLandmark *mark=&draw_landmarks[n];
              gdouble ex=mark->x+mark->w/2.0-old.offset_x;
              gdouble ey=mark->y+mark->h/2.0-old.offset_y;
              gdouble sumx=0,sumy=0,radius=hypot(mark->w,mark->h)/2+2;
              guint count=0;
              cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);
              cairo_user_to_device(cr,&ex,&ey);cairo_destroy(cr);
              /* Central 440x440 ROI contains both complete transformed image
               * positions (diagonal <363), with generous surrounding trails. */
              for (gint y=shell->disp_height/2-220;y<shell->disp_height/2+220;y++)
                for (gint x=shell->disp_width/2-220;x<shell->disp_width/2+220;x++)
                  {
                    guint32 pixel=draw_pixel(surface,x,y);
                    guint rgb[]={(pixel>>16)&255,(pixel>>8)&255,pixel&255};
                    if(rgb[n]>130 && rgb[(n+1)%3]<110 && rgb[(n+2)%3]<110)
                      {
                        g_assert_cmpfloat(hypot(x+0.5-ex,y+0.5-ey),<=,radius);
                        count++;sumx+=x+0.5;sumy+=y+0.5;
                      }
                  }
              g_assert_cmpfloat(count,>=,0.45*mark->w*mark->h);
              g_assert_cmpfloat(count,<=,1.50*mark->w*mark->h);
              g_assert_cmpfloat_with_epsilon(sumx/count,ex,1.0);
              g_assert_cmpfloat_with_epsilon(sumy/count,ey,1.0);
            }
          sx=40-old.offset_x;sy=220-old.offset_y;
          {cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);cairo_user_to_device(cr,&sx,&sy);cairo_destroy(cr);}
          g_assert_cmpfloat_with_epsilon(child->x,sx,1e-8);
          g_assert_cmpfloat_with_epsilon(child->y,sy,1e-8);
          /* GTK allocation is integer; stored image anchor remains exact. */
          g_assert_cmpfloat_with_epsilon(child->matrix.x0,sx,1.0);
          g_assert_cmpfloat_with_epsilon(child->matrix.y0,sy,1.0);
          g_assert_cmpfloat(child->angle,==,0.0);
          g_assert_cmpfloat_with_epsilon(child->matrix.xx,1.0,1e-10);
          g_assert_cmpfloat_with_epsilon(child->matrix.yy,1.0,1e-10);
          /* Actual item paint CTM, independent old origin, not just a center oracle. */
          origin_x=-shell->offset_x;origin_y=-shell->offset_y;
          cairo_matrix_transform_point(&scene.rotated->matrix,&origin_x,&origin_y);
          sx=-old.offset_x;sy=-old.offset_y;
          {cairo_t *cr=legacy_pan_cairo();legacy_pan_set_rotate(&old,cr);cairo_user_to_device(cr,&sx,&sy);cairo_destroy(cr);}
          g_assert_cmpfloat_with_epsilon(origin_x,sx,1e-8);
          g_assert_cmpfloat_with_epsilon(origin_y,sy,1e-8);
          if(step==1 && g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR"))
            {
              GdkWindow *root=gdk_get_default_root_window();
              GdkPixbuf *shot=gdk_pixbuf_get_from_window(root,0,0,
                gdk_window_get_width(root),gdk_window_get_height(root));
              gchar *name=g_strdup_printf("portrait-oncanvas-space-m%u.png",mirror);
              gchar *path=g_build_filename(g_getenv("GIMP_TEST_NAVIGATION_DRAW_DIR"),name,NULL);
              g_assert_cmpint(gdk_window_get_width(root),==,1080);
              g_assert_cmpint(gdk_window_get_height(root),==,1920);
              g_assert_nonnull(shot);
              g_assert_true(gdk_pixbuf_save(shot,path,"png",NULL,NULL));
              g_object_unref(shot);g_free(name);g_free(path);
            }
          cairo_surface_destroy(surface);
        }
      send_event(GDK_KEY_RELEASE,0,0,0,0,GDK_KEY_space);assert_navigation_idle();
      g_object_set(shell->display->config,"space-bar-action",old_space,NULL);
      gtk_widget_destroy(widget);
      draw_scene_free(&scene);
    }
  g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
}

static void old_space_pan_deferred_release(void)
{
  DrawScene scene=draw_scene_new();gint old_space;
  g_object_get(shell->display->config,"space-bar-action",&old_space,NULL);
  g_object_set(shell->display->config,"space-bar-action",GIMP_SPACE_BAR_ACTION_PAN,NULL);
  for(guint n=0;n<2;n++)
    {
      send_event(GDK_KEY_PRESS,0,0,0,0,GDK_KEY_space);assert_grab();
      g_assert_true(shell->painter_navigation_inherited);
      send_event(GDK_KEY_RELEASE,0,GDK_BUTTON1_MASK,0,0,GDK_KEY_space);
      g_assert_false(shell->space_release_pending);
      g_assert_true(shell->button1_release_pending);
      g_assert_null(shell->grab_seat);
      send_event(GDK_BUTTON_RELEASE,1,GDK_BUTTON1_MASK,shell->disp_width/2,shell->disp_height/2,0);
      assert_navigation_idle();g_assert_false(shell->button1_release_pending);
      g_assert_cmpfloat(shell->painter_pan_start_x,==,0);
      g_assert_cmpfloat(shell->painter_pan_start_y,==,0);
    }
  g_object_set(shell->display->config,"space-bar-action",old_space,NULL);
  draw_scene_free(&scene);
}
