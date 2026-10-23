/* Applied zoom fixture is independent pinned old source, not native geometry. */
#include "../../migration/fixtures/legacy-navigation-applied/zoom-oracle.inc"
static void old_applied_zoom_origin(void)
{
  DragScene scene=drag_scene_new(&drag_angles[1],FALSE,1.5,TRUE,real_tool_names[0]);
  LegacyAppliedZoom old={0};
  LegacyPanShell view={0};
  LegacyAZPointerInput input={0};
  DragPoint start={scene.width/2+40,scene.height/2};
  DragPoint motion={scene.width/2+80,scene.height/2};
  gdouble ox,oy,x,y,actual_x,actual_y;
  old.disp_width=scene.width;old.disp_height=scene.height;
  old.image_width=old.image_height=256;old.rotate_angle=37;old.mirrored=FALSE;
  old.scale_x=shell->scale_x;old.scale_y=shell->scale_y;
  old.factor=gimp_zoom_model_get_factor(shell->zoom);old.dot_for_dot=shell->dot_for_dot;
  old.monitor_xres=shell->monitor_xres;old.monitor_yres=shell->monitor_yres;
  gimp_image_get_resolution(gimp_display_get_image(shell->display),&old.image_xres,&old.image_yres);
  g_object_get(shell->zoom,"minimum",&old.minimum,"maximum",&old.maximum,NULL);
  view.disp_width=scene.width;view.disp_height=scene.height;view.rotate_angle=37;
  gimp_display_shell_transform_xy_f(shell,0,0,&ox,&oy);
  legacy_pan_inverse(&view,&ox,&oy);old.offset_x=-ox;old.offset_y=-oy;
  drag_send_pointer(GDK_BUTTON_PRESS,2,GDK_CONTROL_MASK,start);assert_grab();
  input=(LegacyAZPointerInput){(gint)motion.x,(gint)motion.y,LEGACY_AZ_EVENT_CANVAS,TRUE,(gint)motion.x,(gint)motion.y};
  g_assert_null(shell->zoom_focus_point);shell->zoom_focus_point=g_slice_new(GdkPoint);
  shell->zoom_focus_point->x=motion.x;shell->zoom_focus_point->y=motion.y;
  legacy_applied_zoom_apply(&old,(80+1.5*300-40)/300.0,LEGACY_AZ_RETAIN_CENTERING,input);
  drag_send_pointer(GDK_MOTION_NOTIFY,0,GDK_BUTTON2_MASK,motion);
  g_assert_null(shell->zoom_focus_point);g_assert_true(old.queue_consumed);
  g_assert_cmpfloat_with_epsilon(gimp_zoom_model_get_factor(shell->zoom),old.factor,1e-12);
  view.offset_x=old.offset_x;view.offset_y=old.offset_y;
  x=RINT(128*old.scale_x)-old.offset_x;y=RINT(128*old.scale_y)-old.offset_y;
  {cairo_t*cr=legacy_pan_cairo();legacy_pan_set_rotate(&view,cr);cairo_user_to_device(cr,&x,&y);cairo_destroy(cr);}
  gimp_display_shell_transform_xy_f(shell,128,128,&actual_x,&actual_y);
  g_test_message("old applied zoom factor=%.17g oldoffset=%.17g,%.17g expected=%.17g,%.17g actual=%.17g,%.17g",old.factor,old.offset_x,old.offset_y,x,y,actual_x,actual_y);
  g_assert_cmpfloat_with_epsilon(actual_x,x,1e-8);g_assert_cmpfloat_with_epsilon(actual_y,y,1e-8);
  drag_send_pointer(GDK_BUTTON_RELEASE,2,GDK_BUTTON2_MASK,motion);assert_navigation_idle();
  drag_scene_free(&scene);
}
