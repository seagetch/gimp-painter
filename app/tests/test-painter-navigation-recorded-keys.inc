/* Original25.011: every immutable recorded stationary key transition.
 * Old mirrored angles use the opposite sign to the native rotation convention.
 * No old runtime is regenerated; literal recorded angles remain the oracle. */
static gdouble
recorded_key_native_angle (gdouble old_angle, gboolean mirrored)
{
  gdouble angle=fmod(mirrored?-old_angle:old_angle,360.0);
  return angle<0.0?angle+360.0:angle;
}

static void
recorded_key_stationary (gconstpointer data)
{
  guint id=GPOINTER_TO_UINT(data),tool=id/2,mirror=id%2;
  const gchar*root=g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR");
  gchar *path,*contents,*checksum,**lines;GError*error=NULL;
  guint scenes=0,events=0,keys=0,motions=0;
  gboolean previous_ui;LegacyAppliedRotation old={0};
  g_assert_nonnull(root);
  path=g_build_filename(root,"migration","fixtures","legacy-rotation-operations","gui-captures","rotation.tsv",NULL);
  g_assert_true(g_file_get_contents(path,&contents,NULL,&error));g_assert_no_error(error);
  checksum=g_compute_checksum_for_string(G_CHECKSUM_SHA256,contents,-1);
  g_assert_cmpstr(checksum,==,"bfc8b4ab8f532f5b8e0d97f2dd7cace9eec1aefe7a4cd532dbf4e4162cd9d46a");
  lines=g_strsplit(contents,"\n",-1);
  g_object_get(gimp->config,"painter-canvas-ui",&previous_ui,NULL);g_object_set(gimp->config,"painter-canvas-ui",TRUE,NULL);
  DrawScene scene=draw_scene_new();select_start_tool(real_tool_names[tool]);
  gimp_painter_canvas_ui_set_visible(shell,TRUE);g_assert_true(gimp_painter_canvas_ui_get_visible(shell));
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(shell))),1040,1800);path_damage_wait_layout();
  gimp_canvas_item_set_visible(scene.items,TRUE);
  for(guint i=1;lines[i]&&*lines[i];i++)
    {
      gchar**r=g_strsplit(lines[i],"\t",-1);g_assert_cmpuint(g_strv_length(r),==,24);
      if(!g_str_has_prefix(r[1],"stationary-")||atoi(r[11])!=(gint)mirror){g_strfreev(r);continue;}
      gint type=atoi(r[3]);guint state=g_ascii_strtoull(r[4],NULL,10),detail=g_ascii_strtoull(r[7],NULL,10);
      gdouble literal=g_ascii_strtod(r[12],NULL);
      gint x=atoi(r[5])-atoi(r[9])/2+shell->disp_width/2;
      gint y=atoi(r[6])-atoi(r[10])/2+shell->disp_height/2;
      if(type==-1)
        {
          assert_navigation_idle();g_assert_cmpstr(r[2],==,"setup");
          gimp_display_shell_flip(shell,mirror,FALSE);
          gimp_display_shell_rotate_to(shell,recorded_key_native_angle(literal,mirror));
          gimp_display_shell_scroll_set_offset(shell,128-shell->disp_width/2,128-shell->disp_height/2);
          old=(LegacyAppliedRotation){0};old.view.disp_width=shell->disp_width;old.view.disp_height=shell->disp_height;
          old.view.mirrored=mirror;old.view.rotate_angle=literal;gtk_widget_grab_focus(shell->canvas);scenes++;
        }
      else
        {
          g_assert_true(type==GDK_BUTTON_PRESS||type==GDK_MOTION_NOTIFY||type==GDK_BUTTON_RELEASE||type==GDK_KEY_PRESS||type==GDK_KEY_RELEASE);
          if(type==GDK_BUTTON_PRESS)legacy_applied_rotation_begin(&old,NULL,x,y);
          else if(type==GDK_MOTION_NOTIFY)
            {GdkEventMotion event={0};event.state=state;legacy_applied_rotation_motion(&old,(const GdkEvent*)&event,x,y);motions++;}
          else if(type==GDK_BUTTON_RELEASE)legacy_applied_rotation_end(&old,NULL);
          else keys++;
          /* The unchanged old functions validate the retained literal motion
           * record; key transitions leave its angle unchanged until motion. */
          g_assert_cmpfloat_with_epsilon(old.view.rotate_angle,literal,1e-9);
          send_event((GdkEventType)type,(type==GDK_BUTTON_PRESS||type==GDK_BUTTON_RELEASE)?detail:0,state,x,y,(type==GDK_KEY_PRESS||type==GDK_KEY_RELEASE)?detail:0);
          events++;
          if(type==GDK_BUTTON_RELEASE)assert_navigation_idle();else assert_grab();
        }
      gdouble expected=recorded_key_native_angle(literal,mirror);
      g_assert_cmpfloat_with_epsilon(shell->rotate_angle,expected,1e-9);
      g_assert_cmpint(shell->flip_horizontally,==,mirror);g_assert_false(shell->flip_vertically);
      /* Real idle/frame processing cannot repair a wrong expected angle. */
      gimp_test_run_temp_mainloop(10);
      g_assert_cmpfloat_with_epsilon(shell->rotate_angle,expected,1e-9);
      GimpStatusbar*status=gimp_display_shell_get_statusbar(shell);
      g_assert_cmpint(gtk_widget_get_visible(status->rotate_widget),==,expected!=0.0);
      if(expected!=0.0)
        {gchar*label=g_strdup_printf(" %.2f\xC2\xB0",expected);g_assert_cmpstr(gtk_label_get_text(GTK_LABEL(status->rotate_label)),==,label);g_free(label);}
      if(type!=-1)g_test_message("recorded key tool=%u mirror=%u seq=%s scene=%s step=%s type=%d old=%.17g native=%.17g",tool,mirror,r[0],r[1],r[2],type,literal,shell->rotate_angle);
      if(tool==0&&mirror==0&&g_str_equal(r[1],"stationary-m0-i2")&&(g_str_equal(r[2],"free")||g_str_equal(r[2],"snap")))
        {GdkPixbuf*frame=grid_window_capture();g_object_unref(frame);grid_damage_screenshot("recorded-keys",r[2]);}
      g_strfreev(r);
    }
  g_assert_cmpuint(scenes,==,11);g_assert_cmpuint(events,==,99);g_assert_cmpuint(keys,==,33);g_assert_cmpuint(motions,==,44);
  g_test_message("recorded key totals tool=%u mirror=%u scenes=%u events=%u keys=%u motions=%u",tool,mirror,scenes,events,keys,motions);
  draw_scene_free(&scene);g_object_set(gimp->config,"painter-canvas-ui",previous_ui,NULL);
  g_strfreev(lines);g_free(checksum);g_free(contents);g_free(path);
}
