/* Original23.002: first successful old start seeds before the opacity gate.
 * Explicit bitmap and opacity dynamics, no hardware/old runtime claim. */
#include "core/gimpdynamicsoutput.h"
#include "../../migration/fixtures/legacy-smudge-initial-order/oracle.inc"
static unsigned initial_positive_dabs;
static void (*initial_real_paint)(GimpPaintCore*,GList*,GimpPaintOptions*,GimpSymmetry*,GimpPaintState,guint32);
static void initial_observe_paint(GimpPaintCore*core,GList*drawables,GimpPaintOptions*options,GimpSymmetry*sym,GimpPaintState phase,guint32 time)
{
  if(phase==GIMP_PAINT_STATE_MOTION && core->cur_coords.pressure>0)++initial_positive_dabs;
  initial_real_paint(core,drawables,options,sym,phase,time);
}
static void initial_zero_opacity_sampling ()
{
  legacy_initial_source_order();
  for(int clipped=0;clipped<4;++clipped)
    {
      Scene s;GError*error=nullptr;
      g_test_message("fixture original brush spacing=%d",gimp_brush_get_spacing(gimp_context_get_brush(GIMP_CONTEXT(s.options))));
      gimp_brush_set_spacing(gimp_context_get_brush(GIMP_CONTEXT(s.options)),10);
      g_object_set(s.options,"rate",100.0,"use-color-blending",FALSE,"brush-spacing",.1,nullptr);
      g_object_set(gimp_dynamics_get_output(gimp_context_get_dynamics(GIMP_CONTEXT(s.options)),GIMP_DYNAMICS_OUTPUT_OPACITY),"use-pressure",TRUE,nullptr);
      /* Coordinates are image-relative; Scene's drawable starts at3,5. */
      s.coords.x=clipped==0?19:clipped==1?3:clipped==3?1:103;s.coords.y=21;s.coords.pressure=0;
      auto*klass=GIMP_PAINT_CORE_GET_CLASS(s.core);initial_real_paint=klass->paint;klass->paint=initial_observe_paint;initial_positive_dabs=0;
      s.begin();
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,0,&error));g_assert_no_error(error);
      g_assert(s.pixels()==s.initial);
      /* Cross the distinct-color boundary with opacity still zero: no paint
       * or accumulator update, but successful old start has retained its seed. */
      s.coords.x=clipped==2?11:43;
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,20,&error));g_assert_no_error(error);
      g_assert(s.pixels()==s.initial);
      for(unsigned repeat=0;repeat<3;++repeat){g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,21+repeat,&error));g_assert_no_error(error);g_assert(s.pixels()==s.initial);}
      /* Explicit10% spacing; the observer below proves visible native dabs. */
      s.coords.x+=8;s.coords.pressure=1;
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,40,&error));g_assert_no_error(error);
      /* A second segment at constant opacity1 avoids testing a partially
       * interpolated opacity ramp as though it were an opaque old byte. */
      s.coords.x+=8;
      g_assert_true(gimp_painter_smudge_motion(s.core,&s.coords,60,&error));g_assert_no_error(error);
      s.finish(true);klass->paint=initial_real_paint;
      g_test_message("actual positive-pressure native Smudge dabs=%u",initial_positive_dabs);
      g_assert_cmpuint(initial_positive_dabs,>,0);
      auto pixels=s.pixels();const int x=int(s.coords.x)-3,y=int(s.coords.y)-5;
      const int expected=clipped==2?220:40;
      g_test_message("old initial seed variant=%d target=(%d,%d) expectedR=%d actualR=%d",clipped,x,y,expected,pixels[(y*64+x)*4]);
      g_assert_cmpint(pixels[(y*64+x)*4],==,expected);
      unsigned changed=0;for(unsigned n=0;n<pixels.size();++n)if(pixels[n]!=s.initial[n])++changed;
      g_assert_cmpuint(changed,>,0);g_test_message("initial seed changed native bytes=%u",changed);
      g_assert_cmpint(pixels[(y*64+x)*4+1],==,120);g_assert_cmpint(pixels[(y*64+x)*4+2],==,70);
      g_assert_cmpint(pixels[(y*64+x)*4+3],==,255);
      g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);
      g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==pixels);
    }
}
