/* Space PAN is an old configured action, not a custom mouse-button binding.
 * Its original coordinate boundary is inverse-before-gint, independent of
 * modifiers held when Space starts or while it remains down. */
#include "widgets/gimpdevices.h"
#include "widgets/gimpdevicemanager.h"
#include "widgets/gimpdeviceinfo.h"
#include "widgets/gimpdeviceinfo-coords.h"
static void old_space_pan_origin(void)
{
  const guint states[]={0,GDK_SHIFT_MASK,GDK_CONTROL_MASK,GDK_SHIFT_MASK|GDK_CONTROL_MASK,GDK_MOD1_MASK};
  for(guint angle=0;angle<G_N_ELEMENTS(drag_angles);angle++)
  for(guint mirror=0;mirror<2;mirror++)
  for(guint vertical=0;vertical<2;vertical++)
    {
      DragScene scene=drag_scene_new(&drag_angles[angle],mirror,1.5,TRUE,real_tool_names[0]);
      gint old_space;
      if(vertical){gimp_display_shell_flip(shell,mirror,TRUE);gimp_display_shell_rotate_to(shell,scene.angle->angle);}
      g_object_get(shell->display->config,"space-bar-action",&old_space,NULL);
      g_object_set(shell->display->config,"space-bar-action",GIMP_SPACE_BAR_ACTION_PAN,NULL);
      for(guint entry=0;entry<G_N_ELEMENTS(states);entry++)
        {
          LegacyAppliedZoom old=old_zoom_view();LegacyPanShell pan={0};
          GimpDeviceInfo*info=gimp_device_manager_get_current_device(gimp_devices_get_manager(gimp));
          GimpCoords coords;gdouble sx,sy;
          gimp_device_info_get_device_coords(info,gtk_widget_get_window(shell->canvas),&coords);
          pan.disp_width=old.disp_width;pan.disp_height=old.disp_height;
          pan.image_width=old.image_width;pan.image_height=old.image_height;
          pan.rotate_angle=old.rotate_angle;pan.mirrored=old.mirrored;
          pan.scale_x=old.scale_x;pan.scale_y=old.scale_y;pan.offset_x=old.offset_x;pan.offset_y=old.offset_y;
          sx=coords.x;sy=coords.y;legacy_pan_inverse(&pan,&sx,&sy);legacy_pan_begin(&pan,NULL,sx,sy);
          send_event(GDK_KEY_PRESS,0,states[entry],0,0,GDK_KEY_space);assert_grab();
          for(guint step=0;step<G_N_ELEMENTS(states);step++)
            {
              DragPoint motion={coords.x+17.6+step*3.3,coords.y-10.6+step*2.1};
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
      if(vertical)gimp_display_shell_flip(shell,mirror,FALSE);
      gimp_display_shell_rotate_to(shell,scene.angle->angle);
      drag_scene_free(&scene);
    }
}
