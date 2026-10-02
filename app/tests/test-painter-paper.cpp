/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <limits>
#include <cmath>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "libgimpmath/gimpmath.h"
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpcontext.h"
#include "core/gimppattern.h"
#include "core/gimppatternclipboard.h"
#include "core/gimptempbuf.h"
#include "core/gimpbrush.h"
#include "core/gimpbrush-private.h"
#include "core/gimpdynamics.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "paint/gimppaintbrush.h"
#include "paint/gimppaintoptions.h"
#include "paint/gimppainterpaper.h"
#include "paint/gimppainterpaper-paste.h"
#include "tests.h"
}
#include "paint/painter-mypaint-surface/gimp-resources.hpp"
static Gimp *gimp;
static GimpPattern *paper (int channels) {
  auto *p=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","paper oracle",nullptr));
  const char *formats[]={"Y' u8","Y'A u8","R'G'B' u8","R'G'B'A u8"};
  g_clear_pointer(&p->mask,gimp_temp_buf_unref);p->mask=gimp_temp_buf_new(5,3,babl_format(formats[channels-1]));
  auto *data=gimp_temp_buf_get_data(p->mask);
  for(int y=0;y<3;++y)for(int x=0;x<5;++x)for(int c=0;c<channels;++c)data[(y*5+x)*channels+c]=(x*57+y*31+c*83)%256;
  return p;
}
static std::string fixture (const char *name="legacy-paper") {
  auto *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures",name,"pixels.tsv.gz",nullptr);
  GError *error=nullptr;gchar *compressed=nullptr;gsize length=0;
  g_assert_true(g_file_get_contents(path,&compressed,&length,&error));g_assert_no_error(error);g_free(path);
  auto *memory=g_memory_input_stream_new_from_data(compressed,length,g_free);
  auto *decompress=g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP);
  auto *stream=g_converter_input_stream_new(memory,G_CONVERTER(decompress));
  std::string output;char buffer[8192];gssize count;
  while((count=g_input_stream_read(stream,buffer,sizeof buffer,nullptr,&error))>0)output.append(buffer,count);
  g_assert_no_error(error);g_assert_cmpint(count,==,0);g_object_unref(stream);g_object_unref(decompress);g_object_unref(memory);
  return output;
}
static std::vector<guchar> decode (const std::string& hex) {
  g_assert_cmpuint(hex.size()%2,==,0);std::vector<guchar> value(hex.size()/2);
  for(std::size_t i=0;i<value.size();++i){int hi=g_ascii_xdigit_value(hex[2*i]),lo=g_ascii_xdigit_value(hex[2*i+1]);g_assert_cmpint(hi,>=,0);g_assert_cmpint(lo,>=,0);value[i]=hi*16+lo;}
  return value;
}
static void check_old_masks (const char *name, int expected) {
  std::istringstream input(fixture(name));std::string line;std::vector<guchar> raw;int cases=0;
  while(std::getline(input,line)){
    std::istringstream row(line);std::string tag,hex;int id,textured,ch,width,height;double x,y;
    row>>tag;if(tag!="PAPER_MASK")continue;
    row>>id>>textured>>ch>>width>>height>>x>>y>>hex;g_assert_false(row.fail());auto bytes=decode(hex);
    g_assert_cmpuint(bytes.size(),==,gsize(width)*height);
    if(!textured){raw=std::move(bytes);continue;}
    auto *mask=gimp_temp_buf_new(width,height,babl_format("Y u8"));std::memcpy(gimp_temp_buf_get_data(mask),raw.data(),raw.size());
    auto *pattern=paper(ch);GError *error=nullptr;
    auto *result=gimp_painter_paper_texturize(pattern,mask,x,y,&error);g_assert_no_error(error);g_assert_nonnull(result);
    g_assert_cmpmem(gimp_temp_buf_get_data(result),bytes.size(),bytes.data(),bytes.size());
    g_assert_cmpmem(gimp_temp_buf_get_data(mask),raw.size(),raw.data(),raw.size());
    gimp_temp_buf_unref(result);gimp_temp_buf_unref(mask);g_object_unref(pattern);++cases;
  }
  g_assert_cmpint(cases,==,expected);
}
static void old_masks () {check_old_masks("legacy-paper",216);}
static void old_transformed_masks () {check_old_masks("legacy-paper-transform",1296);}
static void all_byte_products () {
  auto *pattern=paper(1);gimp_temp_buf_unref(pattern->mask);pattern->mask=gimp_temp_buf_new(256,1,babl_format("Y u8"));
  auto *values=gimp_temp_buf_get_data(pattern->mask);for(int i=0;i<256;++i)values[i]=i;
  auto *mask=gimp_temp_buf_new(256,256,babl_format("Y u8"));auto *bytes=gimp_temp_buf_get_data(mask);
  for(int row=0;row<256;++row)std::memset(bytes+row*256,row,256);
  GError *error=nullptr;auto *result=gimp_painter_paper_texturize(pattern,mask,128,128,&error);g_assert_no_error(error);g_assert_nonnull(result);
  auto *out=gimp_temp_buf_get_data(result);
  for(int row=0;row<256;++row)for(int column=0;column<256;++column)g_assert_cmpint(out[row*256+column],==,(row*column+127)/255);
  gimp_temp_buf_unref(result);gimp_temp_buf_unref(mask);g_object_unref(pattern);
}
static void high_precision () {
  auto *pattern=paper(4);auto *mask=gimp_temp_buf_new(7,5,babl_format("Y float"));auto *values=reinterpret_cast<float*>(gimp_temp_buf_get_data(mask));
  for(int i=0;i<35;++i)values[i]=(i+.321f)/35;
  GError *error=nullptr;auto *result=gimp_painter_paper_texturize(pattern,mask,3,2,&error);g_assert_no_error(error);g_assert_nonnull(result);
  g_assert_true(gimp_temp_buf_get_format(result)==babl_format("Y float"));auto *out=reinterpret_cast<float*>(gimp_temp_buf_get_data(result));
  for(int y=0;y<5;++y)for(int x=0;x<7;++x)g_assert_cmpfloat(out[y*7+x],==,values[y*7+x]*(((x%5)*57+(y%3)*31)%256/255.0f));
  gimp_temp_buf_unref(result);gimp_temp_buf_unref(mask);g_object_unref(pattern);
}
static void negative_y () {
  auto *pattern=paper(3);auto *mask=gimp_temp_buf_new(7,5,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(mask),255,35);
  GError *error=nullptr;auto *negative=gimp_painter_paper_texturize(pattern,mask,-.25,-.25,&error);g_assert_no_error(error);
  auto *positive=gimp_painter_paper_texturize(pattern,mask,-.25,2.75,&error);g_assert_no_error(error);
  g_assert_cmpmem(gimp_temp_buf_get_data(negative),35,gimp_temp_buf_get_data(positive),35);
  gimp_temp_buf_unref(negative);gimp_temp_buf_unref(positive);gimp_temp_buf_unref(mask);g_object_unref(pattern);
}
static void invalid () {
  auto *pattern=paper(1);auto *mask=gimp_temp_buf_new(1,1,babl_format("Y u8"));GError *error=nullptr;
  for(double value:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),1e100,-1e100}){
    g_assert_null(gimp_painter_paper_texturize(pattern,mask,value,0,&error));g_assert_nonnull(error);g_clear_error(&error);
    g_assert_null(gimp_painter_paper_texturize(pattern,mask,0,value,&error));g_assert_nonnull(error);g_clear_error(&error);
  }
  gimp_temp_buf_unref(pattern->mask);pattern->mask=gimp_temp_buf_new(2,2,babl_format("Y float"));GimpPainterPaperView view{};
  g_assert_false(gimp_painter_paper_get_view(pattern,&view,&error));g_assert_nonnull(error);g_clear_error(&error);
  gimp_temp_buf_unref(mask);g_object_unref(pattern);
}
static void lifetime_dirty () {
  auto *core=GIMP_BRUSH_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,nullptr));auto *pattern=paper(2);
  auto *mask=gimp_temp_buf_new(7,5,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(mask),255,35);
  g_assert_true(gimp_brush_core_texturize_mask(core,mask,3,2)==mask);gimp_brush_core_set_texture(core,pattern);
  const auto *first=gimp_brush_core_texturize_mask(core,mask,3,2);g_assert_nonnull(first);g_assert_cmpint(gimp_temp_buf_get_data(first)[0],==,0);
  gimp_temp_buf_get_data(pattern->mask)[0]=197;gimp_data_dirty(GIMP_DATA(pattern));
  const auto *second=gimp_brush_core_texturize_mask(core,mask,3,2);g_assert_cmpint(gimp_temp_buf_get_data(second)[0],==,197);
  g_object_add_weak_pointer(G_OBJECT(pattern),reinterpret_cast<gpointer*>(&pattern));g_object_unref(pattern);g_assert_nonnull(pattern);
  gimp_brush_core_set_texture(core,core->texture);g_assert_nonnull(pattern);gimp_brush_core_set_texture(core,nullptr);g_assert_null(pattern);g_assert_null(core->texturized_brush);
  gimp_temp_buf_unref(mask);g_object_unref(core);
}

struct TextureReentry {GimpBrushCore *core;GimpPattern *replacement;bool drop;int calls;};
static void pattern_finalized(gpointer data,GObject*) {
  auto &state=*static_cast<TextureReentry*>(data);++state.calls;
  gimp_brush_core_set_texture(state.core,state.replacement);
  if(state.drop)g_object_unref(state.core);
}
static void texture_signal_lease () {
  for(bool drop:{false,true}){
    auto *core=GIMP_BRUSH_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,nullptr));auto *observed=core;
    g_object_add_weak_pointer(G_OBJECT(core),reinterpret_cast<gpointer*>(&observed));
    auto *first=paper(1),*second=paper(2),*third=paper(3);
    gimp_brush_core_set_texture(core,first);g_object_unref(first);
    TextureReentry state{core,third,drop,0};g_object_weak_ref(G_OBJECT(first),pattern_finalized,&state);
    gimp_brush_core_set_texture(core,second);g_assert_cmpint(state.calls,==,1);
    if(drop)g_assert_null(observed);
    else {g_assert_nonnull(observed);g_assert_true(core->texture==third);g_assert_true(core->subsample_cache_invalid);g_object_unref(core);g_assert_null(observed);}
    g_object_unref(second);g_object_unref(third);
  }
}
static void failed_paste_does_not_fallback () {
  auto *image=gimp_image_new(gimp,32,32,GIMP_RGB,GIMP_PRECISION_U8_NON_LINEAR);
  auto *layer=gimp_layer_new(image,32,32,babl_format("R'G'B'A u8"),"paper failure",1,GIMP_LAYER_MODE_PAINTER_NORMAL);
  gimp_image_add_layer(image,layer,nullptr,0,FALSE);auto *drawable=GIMP_DRAWABLE(layer);
  std::vector<guchar> initial(32*32*4,77);for(std::size_t i=3;i<initial.size();i+=4)initial[i]=255;
  gegl_buffer_set(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,32,32),0,babl_format("R'G'B'A u8"),initial.data(),GEGL_AUTO_ROWSTRIDE);
  auto *options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,"brush-size",7.0,"use-texture",TRUE,nullptr));
  auto *brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","paper failure brush",nullptr));brush->priv->mask=gimp_temp_buf_new(7,7,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(brush->priv->mask),255,49);
  auto *pattern=paper(1);auto *dynamics=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","paper failure dynamics",nullptr));
  gimp_context_set_brush(GIMP_CONTEXT(options),brush);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);gimp_context_set_dynamics(GIMP_CONTEXT(options),dynamics);
  auto *core=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,nullptr));GList list={drawable,nullptr,nullptr};GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16;coords.y=16;GError *error=nullptr;
  g_assert_true(gimp_paint_core_start(core,&list,options,&coords,&error));g_assert_no_error(error);
  int px,py;auto *paint=gimp_paint_core_get_paint_buffer(core,drawable,options,GIMP_LAYER_MODE_PAINTER_NORMAL,&coords,&px,&py,nullptr,nullptr);g_assert_nonnull(paint);
  auto *red=gegl_color_new("red");gegl_buffer_set_color(paint,nullptr,red);g_object_unref(red);
  const auto *mask=gimp_brush_core_get_brush_mask(GIMP_BRUSH_CORE(core),&coords,GIMP_BRUSH_SOFT,.5);g_assert_nonnull(mask);
  g_assert_false(gimp_painter_paper_paste(core,mask,0,0,drawable,1,1,GIMP_LAYER_MODE_NORMAL,GIMP_PAINT_CONSTANT,&error));g_assert_no_error(error);
  g_assert_true(gimp_painter_paper_paste(core,mask,0,0,drawable,1,std::numeric_limits<double>::quiet_NaN(),GIMP_LAYER_MODE_PAINTER_NORMAL,GIMP_PAINT_CONSTANT,&error));g_assert_nonnull(error);g_clear_error(&error);
  const int x1=core->x1,y1=core->y1,x2=core->x2,y2=core->y2;
  g_test_expect_message("Gimp-Paint",G_LOG_LEVEL_WARNING,"*Unable to apply painter paper paint: Invalid paper paint transaction*");
  gimp_brush_core_paste_canvas(GIMP_BRUSH_CORE(core),drawable,&coords,1,std::numeric_limits<double>::quiet_NaN(),GIMP_LAYER_MODE_PAINTER_NORMAL,GIMP_BRUSH_SOFT,.5,GIMP_PAINT_CONSTANT);
  g_test_assert_expected_messages();g_assert_cmpint(core->x1,==,x1);g_assert_cmpint(core->y1,==,y1);g_assert_cmpint(core->x2,==,x2);g_assert_cmpint(core->y2,==,y2);
  std::vector<guchar> after(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,32,32),1,babl_format("R'G'B'A u8"),after.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_assert(after==initial);
  gimp_paint_core_cancel(core,&list);gimp_paint_core_finish(core,&list,FALSE);gimp_paint_core_cleanup(core);
  g_object_unref(core);g_object_unref(options);g_object_unref(brush);g_object_unref(pattern);g_object_unref(dynamics);g_object_unref(image);
}
static void options_roundtrip () {
  auto *options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));g_assert_false(options->use_texture);
  auto *pattern=paper(3);gimp_data_make_internal(GIMP_DATA(pattern),"paper-test-internal");gimp_container_add(gimp_data_factory_get_container(gimp->pattern_factory),GIMP_OBJECT(pattern));
  g_object_set(options,"use-texture",TRUE,"pattern-view-size",64,nullptr);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);
  gchar *serialized=gimp_config_serialize_to_string(GIMP_CONFIG(options),nullptr);g_assert_nonnull(std::strstr(serialized,"(use-texture yes)"));g_assert_nonnull(std::strstr(serialized,"paper oracle"));
  auto *loaded=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));GError *error=nullptr;
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(loaded),serialized,-1,nullptr,&error));g_assert_no_error(error);g_assert_true(loaded->use_texture);g_assert_true(gimp_context_get_pattern(GIMP_CONTEXT(loaded))==pattern);
  auto *copy=GIMP_PAINT_OPTIONS(gimp_config_duplicate(GIMP_CONFIG(options)));g_assert_true(copy->use_texture);
  gimp_config_reset(GIMP_CONFIG(options));g_assert_false(options->use_texture);g_assert_true(copy->use_texture);
  gimp_paint_options_copy_props(copy,options,GIMP_CONTEXT_PROP_MASK_BRUSH);g_assert_true(options->use_texture);
  g_free(serialized);g_object_unref(copy);g_object_unref(loaded);g_object_unref(options);gimp_container_remove(gimp_data_factory_get_container(gimp->pattern_factory),GIMP_OBJECT(pattern));g_object_unref(pattern);
}


typedef struct {GimpPattern parent;} PaperUnknownPattern;
typedef struct {GimpPatternClass parent;} PaperUnknownPatternClass;
static GType paper_unknown_pattern_get_type(void);
G_DEFINE_TYPE(PaperUnknownPattern,paper_unknown_pattern,GIMP_TYPE_PATTERN)
static void paper_unknown_pattern_class_init(PaperUnknownPatternClass*){}
static void paper_unknown_pattern_init(PaperUnknownPattern*){}
static void clipboard_gone(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void clipboard_preview_snapshot () {
  using namespace GimpPainter::MyPaint;
  auto *context=gimp_context_new(gimp,"clipboard paper",nullptr);
  auto *original=GIMP_PATTERN(gimp_pattern_clipboard_new(gimp));
  g_clear_pointer(&original->mask,gimp_temp_buf_unref);original->mask=gimp_temp_buf_new(3,5,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(original->mask),255,15);
  gimp_context_set_pattern(context,original);Resource settings;settings.set_switch(BRUSH_USE_GIMP_TEXTURE,true);
  GimpResources snapshot(context,settings,GimpResources::Purpose::Preview);
  auto render=[](GimpResources &resources){
    auto *buffer=gegl_buffer_new(GEGL_RECTANGLE(0,0,16,16),babl_format("R'G'B'A u8"));
    {GeglSurface surface(buffer);resources.attach(surface);surface.begin_session();surface.draw_dab(8,8,4,1,0,0,1,1);surface.end_session();}
    std::vector<guchar> result(16*16*4);gegl_buffer_get(buffer,GEGL_RECTANGLE(0,0,16,16),1,babl_format("R'G'B'A u8"),result.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);g_object_unref(buffer);return result;
  };
  const auto expected=render(snapshot);bool painted=false;for(std::size_t i=3;i<expected.size();i+=4)painted|=expected[i]>0;g_assert_true(painted);
  std::memset(gimp_temp_buf_get_data(original->mask),0,15);gimp_data_dirty(GIMP_DATA(original));
  g_assert(render(snapshot)==expected);GimpResources fresh(context,settings,GimpResources::Purpose::Preview);g_assert(render(fresh)!=expected);
  auto *unknown=GIMP_PATTERN(g_object_new(paper_unknown_pattern_get_type(),"name","unknown paper subclass",nullptr));g_clear_pointer(&unknown->mask,gimp_temp_buf_unref);unknown->mask=gimp_temp_buf_new(3,5,babl_format("Y u8"));std::memset(gimp_temp_buf_get_data(unknown->mask),255,15);gimp_context_set_pattern(context,unknown);
  bool rejected=false;try{GimpResources unsupported(context,settings,GimpResources::Purpose::Preview);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);
  bool gone=false;g_object_weak_ref(G_OBJECT(original),clipboard_gone,&gone);g_object_unref(original);g_assert_true(gone);
  g_assert(render(snapshot)==expected);g_object_unref(unknown);g_object_unref(context);
}
static void native_float_stroke () {
  std::vector<float> first;
  for(bool textured:{false,true}) {
    auto *image=gimp_image_new(gimp,32,32,GIMP_RGB,GIMP_PRECISION_FLOAT_NON_LINEAR);
    auto *layer=gimp_layer_new(image,32,32,babl_format("R'G'B'A float"),"float paper",1,GIMP_LAYER_MODE_NORMAL);
    gimp_image_add_layer(image,layer,nullptr,0,FALSE);auto *drawable=GIMP_DRAWABLE(layer);
    std::vector<float> initial(32*32*4);
    for(std::size_t i=0;i<initial.size();i+=4){initial[i]=.123456f;initial[i+1]=.314159f;initial[i+2]=.718281f;initial[i+3]=.876543f;}
    gegl_buffer_set(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,32,32),0,babl_format("R'G'B'A float"),initial.data(),GEGL_AUTO_ROWSTRIDE);
    auto *options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,"brush-size",7.0,"use-texture",textured,nullptr));
    auto *brush=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","float paper brush",nullptr));
    brush->priv->mask=gimp_temp_buf_new(7,7,babl_format("Y float"));auto *mask=reinterpret_cast<float*>(gimp_temp_buf_get_data(brush->priv->mask));
    for(int i=0;i<49;++i)mask[i]=.789123f;
    auto *pattern=paper(1);std::memset(gimp_temp_buf_get_data(pattern->mask),255,15);
    auto *dynamics=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","float paper dynamics",nullptr));
    gimp_context_set_brush(GIMP_CONTEXT(options),brush);gimp_context_set_pattern(GIMP_CONTEXT(options),pattern);gimp_context_set_dynamics(GIMP_CONTEXT(options),dynamics);
    auto *fg=gegl_color_new("red");gimp_context_set_foreground(GIMP_CONTEXT(options),fg);g_object_unref(fg);gimp_context_set_opacity(GIMP_CONTEXT(options),.63);
    auto *core=GIMP_PAINT_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,nullptr));GList drawables={drawable,nullptr,nullptr};GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=16.25;coords.y=16.25;GError *error=nullptr;
    gimp_image_undo_free(image);g_assert_true(gimp_paint_core_start(core,&drawables,options,&coords,&error));g_assert_no_error(error);
    gimp_paint_core_paint(core,&drawables,options,GIMP_PAINT_STATE_INIT,0);gimp_paint_core_paint(core,&drawables,options,GIMP_PAINT_STATE_MOTION,0);gimp_paint_core_paint(core,&drawables,options,GIMP_PAINT_STATE_FINISH,1);gimp_paint_core_finish(core,&drawables,TRUE);
    auto pixels=[&]{std::vector<float> values(initial.size());gegl_buffer_get(gimp_drawable_get_buffer(drawable),GEGL_RECTANGLE(0,0,32,32),1,babl_format("R'G'B'A float"),values.data(),GEGL_AUTO_ROWSTRIDE,GEGL_ABYSS_NONE);return values;};
    const auto final=pixels();g_assert(final!=initial);if(textured)g_assert(final==first);else first=final;
    g_assert_cmpfloat(final[0],==,.123456f);g_assert_true(gimp_image_undo(image));g_assert(pixels()==initial);g_assert_true(gimp_image_redo(image));g_assert(pixels()==final);
    gimp_paint_core_cleanup(core);g_object_unref(core);g_object_unref(options);g_object_unref(brush);g_object_unref(pattern);g_object_unref(dynamics);g_object_unref(image);
  }
}
int main(int argc,char **argv){g_test_init(&argc,&argv,nullptr);gimp=gimp_init_for_testing();
  g_test_add_func("/painter/paper/old-native-masks",old_masks);
  g_test_add_func("/painter/paper/old-transformed-mask-kernel",old_transformed_masks);
  g_test_add_func("/painter/paper/all-byte-products",all_byte_products);
  g_test_add_func("/painter/paper/float-mask",high_precision);
  g_test_add_func("/painter/paper/negative-y-extension",negative_y);
  g_test_add_func("/painter/paper/invalid-domain",invalid);
  g_test_add_func("/painter/paper/lifetime-dirty",lifetime_dirty);
  g_test_add_func("/painter/paper/texture-signal-lease",texture_signal_lease);
  g_test_add_func("/painter/paper/failed-paste-no-fallback",failed_paste_does_not_fallback);
  g_test_add_func("/painter/paper/options-roundtrip",options_roundtrip);
  g_test_add_func("/painter/paper/native-float-undo",native_float_stroke);
  g_test_add_func("/painter/paper/clipboard-preview-snapshot",clipboard_preview_snapshot);
  return g_test_run();}
