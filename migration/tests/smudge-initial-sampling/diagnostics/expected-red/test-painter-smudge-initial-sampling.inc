/* Original23.002: first successful old start seeds before the opacity gate.
 * Explicit bitmap and opacity dynamics, no hardware/old runtime claim. */
#include "core/gimpdynamicsoutput.h"
static void initial_zero_opacity_sampling ()
{
  for(int clipped=0;clipped<3;++clipped)
    {
      Scene s;GError*error=nullptr;
      g_object_set(s.options,"rate",100.0,"use-color-blending",FALSE,nullptr);
      g_object_set(gimp_dynamics_get_output(gimp_context_get_dynamics(GIMP_CONTEXT(s.options)),GIMP_DYNAMICS_OUTPUT_OPACITY),"use-pressure",TRUE,nullptr);
      /* Coordinates are image-relative; Scene's drawable starts at3,5. */
      s.coords.x=clipped==0?19:clipped==1?3:103;s.coords.y=21;s.coords.pressure=0;
      s.begin();
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,0,&error));g_assert_no_error(error);
      g_assert(s.pixels()==s.initial);
      /* Cross the distinct-color boundary with opacity still zero: no paint
       * or accumulator update, but successful old start has retained its seed. */
      s.coords.x=clipped==2?19:51;
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,20,&error));g_assert_no_error(error);
      g_assert(s.pixels()==s.initial);
      s.coords.x+=1;s.coords.pressure=1;
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,40,&error));g_assert_no_error(error);
      s.finish(true);
      auto pixels=s.pixels();const int x=int(s.coords.x)-3,y=int(s.coords.y)-5;
      const int expected=clipped==2?220:40;
      g_test_message("old initial seed variant=%d target=(%d,%d) expectedR=%d actualR=%d",clipped,x,y,expected,pixels[(y*64+x)*4]);
      g_assert_cmpint(pixels[(y*64+x)*4],==,expected);
      g_assert_cmpint(pixels[(y*64+x)*4+1],==,120);g_assert_cmpint(pixels[(y*64+x)*4+2],==,70);
      g_assert_cmpint(pixels[(y*64+x)*4+3],==,255);
      g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);
      g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==pixels);
    }
}
