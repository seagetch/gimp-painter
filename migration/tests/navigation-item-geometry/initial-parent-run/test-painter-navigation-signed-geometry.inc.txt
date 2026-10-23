/* Signed rectangle representations normalize to identical native geometry. */
static void signed_rectangle_geometry(gconstpointer data)
{
  guint id=GPOINTER_TO_UINT(data),a=id%3;gboolean guides=id>=3;DrawScene scene=draw_scene_new();guint scenes=0;
  for(guint h=0;h<2;h++)for(guint v=0;v<2;v++)
    {
      gimp_display_shell_flip(shell,h,v);gimp_display_shell_rotate_to(shell,drag_angles[a].angle);
      for(guint variant=0;variant<(guides?24:2);variant++)
        {
          GimpCanvasItem*base=guides?gimp_canvas_rectangle_guides_new(shell,45,55,165,145,(GimpGuidesType)(variant%8),1+8*(variant/8)):gimp_canvas_rectangle_new(shell,45,55,165,145,variant);
          cairo_surface_t*reference=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,shell->disp_width,shell->disp_height);cairo_t*cr=cairo_create(reference);
          if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);gimp_canvas_item_draw(base,cr);cairo_destroy(cr);cairo_surface_flush(reference);
          cairo_region_t*base_extent=gimp_canvas_item_get_extents(base);
          for(guint nx=0;nx<2;nx++)for(guint ny=0;ny<2;ny++)
            {
              gdouble x=45+(nx?165:0),y=55+(ny?145:0),w=nx?-165:165,hh=ny?-145:145;
              GimpCanvasItem*item=guides?gimp_canvas_rectangle_guides_new(shell,x,y,w,hh,(GimpGuidesType)(variant%8),1+8*(variant/8)):gimp_canvas_rectangle_new(shell,x,y,w,hh,variant);
              cairo_surface_t*actual=cairo_image_surface_create(CAIRO_FORMAT_ARGB32,shell->disp_width,shell->disp_height);cr=cairo_create(actual);
              if(shell->rotate_transform)cairo_transform(cr,shell->rotate_transform);gimp_canvas_item_draw(item,cr);cairo_destroy(cr);cairo_surface_flush(actual);
              g_assert_cmpmem(cairo_image_surface_get_data(actual),cairo_image_surface_get_stride(actual)*shell->disp_height,cairo_image_surface_get_data(reference),cairo_image_surface_get_stride(reference)*shell->disp_height);
              cairo_region_t*extent=gimp_canvas_item_get_extents(item);
              if(base_extent){g_assert_nonnull(extent);g_assert_true(cairo_region_equal(base_extent,extent));}else g_assert_null(extent);
              if(extent)cairo_region_destroy(extent);cairo_surface_destroy(actual);g_object_unref(item);scenes++;
            }
          if(base_extent)cairo_region_destroy(base_extent);cairo_surface_destroy(reference);g_object_unref(base);
        }
    }
  g_test_message("signed geometry guides=%u angle=%g scenes=%u",guides,drag_angles[a].angle,scenes);g_assert_cmpuint(scenes,==,guides?384:32);draw_scene_free(&scene);
}
