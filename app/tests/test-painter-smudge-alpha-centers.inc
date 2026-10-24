/* Successor initial/update proof: partial alpha and transparent hidden colors.
 * Reuses exact old-source arithmetic and forward-only native center observer. */
static void smudge_alpha_centers()
{
  unsigned scenes=0,updates=0,transparent_changed=0;
  for(int alpha_mode: {1,2})for(int side=17;side<=18;++side)for(int paper: {0,128})
    for(unsigned channels: {2u,4u})for(int entry=0;entry<4;++entry)for(int rate=0;rate<=100;rate+=50){
      CenterScene s(channels,side,paper);
      for(int y=0;y<48;++y)for(int x=0;x<64;++x)s.initial[(y*64+x)*channels+channels-1]=alpha_mode==2?0:x<32?96:200;
      gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(s.layer)),GEGL_RECTANGLE(0,0,64,48),0,s.format,s.initial.data(),GEGL_AUTO_ROWSTRIDE);
      g_assert(s.pixels()==s.initial); // Native storage retains meaningful hidden colors.
      center_scene=&s;center_seeded=false;center_visible=false;center_callbacks=center_updates=center_outside=center_seeds=0;center_q=(int)std::floor(rate*255.0/100+.5);
      g_object_set(s.options,"rate",(double)rate,nullptr);auto*opacity=gimp_dynamics_get_output(gimp_context_get_dynamics(GIMP_CONTEXT(s.options)),GIMP_DYNAMICS_OUTPUT_OPACITY);g_object_set(opacity,"use-pressure",TRUE,nullptr);
      s.coords.x=entry==0?19:entry==1?3:entry==2?103:1;s.coords.y=21;s.coords.pressure=0;
      auto*klass=GIMP_PAINT_CORE_GET_CLASS(s.core);center_real_paint=klass->paint;klass->paint=center_observe;
      GError*e=nullptr;g_assert_true(gimp_painter_smudge_begin(s.core,GIMP_DRAWABLE(s.layer),s.options,&s.coords,&e));g_assert_no_error(e);s.motion();
      if(entry==2){g_assert_false(center_seeded);g_assert_cmpuint(center_outside,==,1);}else g_assert_true(center_seeded);
      s.coords.x=entry==2?11:43;s.motion();g_assert(s.pixels()==s.initial);g_assert_true(center_seeded);
      g_object_set(opacity,"use-pressure",FALSE,nullptr);center_visible=true;s.coords.x+=8;s.motion();s.coords.x+=8;s.motion();
      g_assert_true(gimp_painter_smudge_finish(s.core,TRUE,&e));g_assert_no_error(e);klass->paint=center_real_paint;
      g_assert_cmpuint(center_seeds,==,1);g_assert_cmpuint(center_updates,>,1);auto final=s.pixels();
      unsigned hidden=0;for(unsigned n=0;n<final.size();n+=channels){if(alpha_mode==2){g_assert_cmpuint(final[n+channels-1],==,0);for(unsigned c=0;c<channels-1;++c)hidden+=final[n+c]!=s.initial[n+c];}}
      if(alpha_mode==2){g_assert_cmpuint(hidden,>,0);transparent_changed+=hidden;}
      g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==final);
      g_test_message("alpha center scene alpha=%d size=%d paper=%d channels=%u entry=%d rate=%d callbacks=%u updates=%u outside=%u hidden_changed=%u",alpha_mode,side,paper,channels,entry,rate,center_callbacks,center_updates,center_outside,hidden);++scenes;updates+=center_updates;
    }
  g_assert_cmpuint(scenes,==,192);g_assert_cmpuint(transparent_changed,>,0);
  g_test_message("alpha matrix192 scenes; actual updated centers=%u; transparent changed hidden bytes=%u",updates,transparent_changed);
}
