/* Bounded old accumulator-center oracle: four native formats, opaque alpha.
 * Forwards every real paint callback unchanged; no production kernel expected. */
#include <cmath>
#include <algorithm>
extern "C" {
#include "core/gimpsymmetry.h"
#include "core/gimppattern.h"
}
namespace LegacySmudgeCenter {
#include "../../migration/fixtures/legacy-smudge-center/paint-funcs-utils.h"
#include "../../migration/fixtures/legacy-smudge-center/blend-pixels.inc"
#include "../../migration/fixtures/legacy-smudge-center/replace-pixels.inc"
#include "../../migration/fixtures/legacy-smudge-center/opaque-compose.inc"
#undef HAS_ALPHA
#undef INT_MULT
#undef INT_MULT3
#undef INT_BLEND
}
struct CenterScene {
  GimpImage* image;GimpLayer* layer;GimpPainterSmudge* core;GimpPaintOptions* options;
  const Babl* format;unsigned channels;int size,paper;std::vector<guchar> initial;
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;
  explicit CenterScene(unsigned c,int side,int texture):channels(c),size(side),paper(texture),initial(64*48*c) {
    format=babl_format(c==1?"Y' u8":c==2?"Y'A u8":c==3?"R'G'B' u8":"R'G'B'A u8");
    image=gimp_image_new(gimp,72,58,c<=2?GIMP_GRAY:GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
    layer=gimp_layer_new(image,64,48,format,"center oracle",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
    gimp_image_add_layer(image,layer,nullptr,0,FALSE);g_object_ref(layer);gimp_item_set_offset(GIMP_ITEM(layer),3,5);
    for(int y=0;y<48;++y)for(int x=0;x<64;++x)for(unsigned k=0;k<c;++k)
      initial[(y*64+x)*c+k]=c%2==0 && k==c-1?255:k==0?(x<32?40:220):k==1?120:70;
    gegl_buffer_set(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,48),0,format,initial.data(),GEGL_AUTO_ROWSTRIDE);
    options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_SMUDGE_OPTIONS,"gimp",gimp,"brush-size",(double)side,"use-color-blending",FALSE,"hard",TRUE,nullptr));
    auto*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","center solid",nullptr));b->priv->mask=gimp_temp_buf_new(side,side,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(b->priv->mask),255,side*side);gimp_brush_set_spacing(b,10);gimp_context_set_brush(GIMP_CONTEXT(options),b);g_object_unref(b);
    if(paper){g_object_set(options,"use-texture",TRUE,nullptr);auto*p=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","center paper",nullptr));g_clear_pointer(&p->mask,gimp_temp_buf_unref);p->mask=gimp_temp_buf_new(5,3,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(p->mask),paper,15);gimp_context_set_pattern(GIMP_CONTEXT(options),p);g_object_unref(p);}
    auto*d=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","center dynamics",nullptr));gimp_context_set_dynamics(GIMP_CONTEXT(options),d);g_object_unref(d);
    core=GIMP_PAINTER_SMUDGE(g_object_new(GIMP_TYPE_PAINTER_SMUDGE,nullptr));
  }
  ~CenterScene(){g_object_unref(core);g_object_unref(options);g_object_unref(layer);g_object_unref(image);}
  std::vector<guchar> pixels(){std::vector<guchar>p(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(GIMP_DRAWABLE(layer)),GEGL_RECTANGLE(0,0,64,48),1,format,p.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return p;}
  void motion(){GError*e=nullptr;g_assert_true(gimp_painter_smudge_motion(core,&coords,0,&e));g_assert_no_error(e);}
};
static CenterScene* center_scene;
static bool center_seeded,center_visible;
static unsigned center_callbacks,center_updates,center_outside,center_seeds;
static int center_q;
static guchar center_accum[4];
static void(*center_real_paint)(GimpPaintCore*,GList*,GimpPaintOptions*,GimpSymmetry*,GimpPaintState,guint32);
static void center_observe(GimpPaintCore*c,GList*d,GimpPaintOptions*o,GimpSymmetry*sym,GimpPaintState phase,guint32 t)
{
  if(phase!=GIMP_PAINT_STATE_MOTION){center_real_paint(c,d,o,sym,phase,t);return;}
  auto&s=*center_scene;GimpCoords pos=*gimp_symmetry_get_origin(sym);
  if(center_callbacks++==0){pos.x-=3;pos.y-=5;}
  const int x=(int)pos.x,y=(int)pos.y;
  /* Pinned old bitmap paint-area border: footprint plus1 on each side. */
  const int left=(int)std::floor(pos.x)-s.size/2-1,top=(int)std::floor(pos.y)-s.size/2-1;
  const bool overlap=left<64 && left+s.size+2>0 && top<48 && top+s.size+2>0;
  auto before=s.pixels();
  if(!center_seeded && overlap){unsigned i=(CLAMP(y,0,47)*64+CLAMP(x,0,63))*s.channels;std::copy_n(before.data()+i,s.channels,center_accum);center_seeded=true;++center_seeds;g_test_message("seed channels=%u at(%g,%g) clamped(%d,%d) value=%u",s.channels,pos.x,pos.y,CLAMP(x,0,63),CLAMP(y,0,47),center_accum[0]);}
  if(!overlap)++center_outside;
  if(center_visible){g_assert_true(overlap);g_assert_true(center_seeded);g_assert_cmpint(x,>=,0);g_assert_cmpint(x,<,64);g_assert_cmpint(y,>=,0);g_assert_cmpint(y,<,48);unsigned i=(y*64+x)*s.channels;LegacySmudgeCenter::blend_pixels(before.data()+i,center_accum,center_accum,center_q,1,s.channels);}
  center_real_paint(c,d,o,sym,phase,t);
  auto* failure=gimp_painter_smudge_dup_error(s.core);if(failure)g_error("native center failure: %s",failure);g_free(failure);
  if(center_visible){const GimpTempBuf*observed=s.paper?GIMP_BRUSH_CORE(c)->texturized_brush:nullptr;if(!observed)for(int a=0;a<2;++a)for(int b=0;b<2;++b)if(GIMP_BRUSH_CORE(c)->solid_brushes[a][b])observed=GIMP_BRUSH_CORE(c)->solid_brushes[a][b];g_assert_nonnull(observed);g_assert_true(gimp_temp_buf_get_format(observed)==babl_format("Y float"));const int w=gimp_temp_buf_get_width(observed),h=gimp_temp_buf_get_height(observed);g_assert_cmpint(w,==,s.size+2);g_assert_cmpint(h,==,s.size+2);g_assert_cmpfloat(reinterpret_cast<const float*>(gimp_temp_buf_get_data(observed))[(h/2)*w+w/2],==,s.paper?s.paper/255.0f:1.0f);}
  auto after=s.pixels();
  if(!center_visible)g_assert(before==after);
  else {unsigned i=(y*64+x)*s.channels;guchar expected[4];std::copy_n(center_accum,s.channels,expected);if(s.channels%2==0){const guchar coverage=s.paper?s.paper:255;const gboolean affect[4]={TRUE,TRUE,TRUE,TRUE};LegacySmudgeCenter::replace_pixels(before.data()+i,center_accum,expected,&coverage,1,255,affect,s.channels,s.channels);}else{guchar paint[4];std::copy_n(center_accum,s.channels,paint);paint[s.channels]=s.paper?s.paper:255;const gboolean affect[4]={TRUE,TRUE,TRUE,TRUE};LegacySmudgeCenter::combine_inten_and_inten_a_pixels(before.data()+i,paint,expected,nullptr,255,affect,1,s.channels);}for(unsigned k=0;k<s.channels;++k){if(after[i+k]!=expected[k])g_test_message("center mismatch c=%u q=%d xy=(%g,%g) k=%u expected=%u actual=%u",s.channels,center_q,pos.x,pos.y,k,expected[k],after[i+k]);g_assert_cmpuint(after[i+k],==,expected[k]);}++center_updates;}
}
static void smudge_center_matrix()
{
  unsigned scenes=0,updates=0;
  for(int side=17;side<=18;++side)for(int paper: {0,128})for(unsigned channels=1;channels<=4;++channels)for(int entry=0;entry<4;++entry)for(int rate=0;rate<=100;rate+=50){
    CenterScene s(channels,side,paper);center_scene=&s;center_seeded=false;center_visible=false;center_callbacks=center_updates=center_outside=center_seeds=0;center_q=(int)std::floor(rate*255.0/100+.5);
    g_object_set(s.options,"rate",(double)rate,nullptr);auto*opacity=gimp_dynamics_get_output(gimp_context_get_dynamics(GIMP_CONTEXT(s.options)),GIMP_DYNAMICS_OUTPUT_OPACITY);g_object_set(opacity,"use-pressure",TRUE,nullptr);
    s.coords.x=entry==0?19:entry==1?3:entry==2?103:1;s.coords.y=21;s.coords.pressure=0;
    auto*klass=GIMP_PAINT_CORE_GET_CLASS(s.core);center_real_paint=klass->paint;klass->paint=center_observe;
    GError*e=nullptr;g_assert_true(gimp_painter_smudge_begin(s.core,GIMP_DRAWABLE(s.layer),s.options,&s.coords,&e));g_assert_no_error(e);s.motion();
    if(entry==2){g_assert_false(center_seeded);g_assert_cmpuint(center_outside,==,1);}else g_assert_true(center_seeded);
    s.coords.x=entry==2?11:43;s.motion();g_assert(s.pixels()==s.initial);g_assert_true(center_seeded);
    /* Disable pressure mapping: each subsequent real dab has opacity1, even
     * while interpolation retains zero pressure. No simulated paint call. */
    g_object_set(opacity,"use-pressure",FALSE,nullptr);center_visible=true;s.coords.x+=8;s.motion();s.coords.x+=8;s.motion();
    g_assert_true(gimp_painter_smudge_finish(s.core,TRUE,&e));g_assert_no_error(e);klass->paint=center_real_paint;
    g_assert_cmpuint(center_seeds,==,1);g_assert_cmpuint(center_updates,>,1);auto final=s.pixels();
    g_assert_true(gimp_image_undo(s.image));g_assert(s.pixels()==s.initial);g_assert_true(gimp_image_redo(s.image));g_assert(s.pixels()==final);
    g_test_message("center scene size=%d paper=%d channels=%u entry=%d rate=%d callbacks=%u updates=%u outside=%u",side,paper,channels,entry,rate,center_callbacks,center_updates,center_outside);++scenes;updates+=center_updates;
  }
  g_assert_cmpuint(scenes,==,192);g_test_message("center matrix192 scenes; actual updated centers=%u; opaque alpha only",updates);
}
