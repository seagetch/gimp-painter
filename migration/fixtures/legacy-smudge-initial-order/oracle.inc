/* Unchanged old orchestration. Start/motion are explicit boundary observers,
 * not fake old pixel algorithms. Numerical seed cases use old fixed-color math. */
struct LegacyInitialOrder {
  gboolean initialized=FALSE;
  guchar *accum_data=nullptr,*blending_data=nullptr;
  bool overlap=true;
  unsigned starts=0,motions=0,visible=0;
  double seed_x=0;
};
static gboolean legacy_initial_start(LegacyInitialOrder*s,GimpDrawable*,GimpPaintOptions*,const GimpCoords*c)
{ ++s->starts;if(!s->overlap)return FALSE;s->seed_x=c->x;return TRUE; }
static void legacy_initial_motion(LegacyInitialOrder*s,GimpDrawable*,GimpPaintOptions*,const GimpCoords*c)
{ g_assert_true(s->initialized);++s->motions;if(c->pressure!=0)++s->visible; }
#define GimpPaintCore LegacyInitialOrder
#define GimpSmudge LegacyInitialOrder
#define GIMP_SMUDGE(value) (value)
#define gimp_smudge_paint legacy_initial_paint
#define gimp_smudge_start legacy_initial_start
#define gimp_smudge_motion legacy_initial_motion
#include "paint-order.inc"
#undef gimp_smudge_motion
#undef gimp_smudge_start
#undef gimp_smudge_paint
#undef GIMP_SMUDGE
#undef GimpSmudge
#undef GimpPaintCore
static void legacy_initial_source_order()
{
  LegacyInitialOrder old;GimpCoords c=GIMP_COORDS_DEFAULT_VALUES;c.x=16;c.pressure=0;
  legacy_initial_paint(&old,nullptr,nullptr,&c,GIMP_PAINT_STATE_MOTION,0);
  g_assert_true(old.initialized);g_assert_cmpuint(old.starts,==,1);g_assert_cmpuint(old.motions,==,1);g_assert_cmpuint(old.visible,==,0);
  c.x=49;c.pressure=1;legacy_initial_paint(&old,nullptr,nullptr,&c,GIMP_PAINT_STATE_MOTION,1);
  g_assert_cmpuint(old.starts,==,1);g_assert_cmpfloat(old.seed_x,==,16);g_assert_cmpuint(old.visible,==,1);
  legacy_initial_paint(&old,nullptr,nullptr,&c,GIMP_PAINT_STATE_FINISH,2);g_assert_false(old.initialized);
  old=LegacyInitialOrder{};old.overlap=false;c.x=100;c.pressure=0;
  legacy_initial_paint(&old,nullptr,nullptr,&c,GIMP_PAINT_STATE_MOTION,0);
  g_assert_false(old.initialized);g_assert_cmpuint(old.motions,==,0);
  old.overlap=true;c.x=63;legacy_initial_paint(&old,nullptr,nullptr,&c,GIMP_PAINT_STATE_MOTION,1);
  g_assert_true(old.initialized);g_assert_cmpuint(old.starts,==,2);g_assert_cmpfloat(old.seed_x,==,63);g_assert_cmpuint(old.visible,==,0);
}
