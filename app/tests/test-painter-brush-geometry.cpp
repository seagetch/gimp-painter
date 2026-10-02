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
#include "core/gimpbrushgenerated.h"
#include "core/gimppainterprofile.h"
#include "core/gimpbrush-private.h"
#include "core/gimpdynamics.h"
#include "core/gimpimage.h"
#include "core/gimpimage-undo.h"
#include "core/gimplayer.h"
#include "core/gimplayer-new.h"
#include "paint/gimppaintbrush.h"
#include "paint/gimppaintoptions.h"
#include "paint/gimppainterpaper.h"
#include "paint/gimppainterbrushgeometry.h"
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
static std::string fixture (const char *name="legacy-paper", const char *file="pixels.tsv.gz") {
  auto *path=g_build_filename(g_getenv("GIMP_TESTING_ABS_TOP_SRCDIR"),"migration/fixtures",name,file,nullptr);
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
static GimpBrush *brush(int shape) {
 if(shape>=3)return GIMP_BRUSH(gimp_brush_generated_new("generated paper brush",(GimpBrushGeneratedShape)(shape-3),8.75,3,.42,1.7,27));
 int w=shape==0?5:shape==1?4:17,h=shape==0?3:shape==1?4:17;
 GimpBrush*b=GIMP_BRUSH(g_object_new(GIMP_TYPE_BRUSH,"name","paper brush",NULL));b->priv->mask=gimp_temp_buf_new(w,h,babl_format("Y u8"));guchar*d=gimp_temp_buf_get_data(b->priv->mask);
 for(int y=0;y<h;++y)for(int x=0;x<w;++x)d[y*w+x]=(x*31+y*67+17)%256;
 return b;
}

static void original_masks () {
  std::istringstream input(fixture("legacy-paper-transform"));std::string line;int cases=0;
  GimpBrushCore *core=nullptr;GimpBrush *selected=nullptr;GimpPattern *pattern=nullptr;
  while(std::getline(input,line)) {
    std::istringstream row(line);std::string tag,hex;int id,textured,ch,width,height;double x,y;
    row>>tag;if(tag!="PAPER_MASK")continue;
    row>>id>>textured>>ch>>width>>height>>x>>y>>hex;g_assert_false(row.fail());auto expected=decode(hex);
    const int shape=(id/54)%6,transform=(id/18)%3,mode=(id/6)%3;
    if(!textured) {
      core=GIMP_BRUSH_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,nullptr));selected=brush(shape);pattern=paper(ch);
      gimp_painter_brush_geometry_enable(core,TRUE);gimp_brush_core_set_brush(core,selected);core->brush=selected;
      core->scale=transform==1?.65:transform==2?1.4:1;core->hardness=1;core->angle=transform==2?.125:0;core->aspect_ratio=transform==2?3:0;
    }
    GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=x;coords.y=y;
    gimp_brush_core_set_texture(core,textured?pattern:nullptr);
    const auto*actual=gimp_brush_core_get_brush_mask(core,&coords,static_cast<GimpBrushApplicationMode>(mode),.37);
    g_assert_nonnull(actual);g_assert_cmpint(gimp_temp_buf_get_width(actual),==,width);g_assert_cmpint(gimp_temp_buf_get_height(actual),==,height);
    std::vector<guchar> bytes(expected.size());babl_process(babl_fish(gimp_temp_buf_get_format(actual),babl_format("Y u8")),gimp_temp_buf_get_data(actual),bytes.data(),bytes.size());
    g_assert_cmpmem(bytes.data(),bytes.size(),expected.data(),expected.size());++cases;
    if(textured){g_object_unref(core);g_object_unref(selected);g_object_unref(pattern);}
  }
  g_assert_cmpint(cases,==,2592);
}
static void options_roundtrip () {
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));
  g_assert_false(gimp_painter_brush_geometry_options(options));
  g_object_set(options,"use-texture",TRUE,nullptr);gimp_context_set_paint_mode(GIMP_CONTEXT(options),GIMP_LAYER_MODE_PAINTER_NORMAL);
  g_assert_false(gimp_painter_brush_geometry_options(options));
  g_object_set(options,"painter-legacy-brush-geometry",TRUE,nullptr);
  auto*duplicate=GIMP_PAINT_OPTIONS(gimp_config_duplicate(GIMP_CONFIG(options)));
  g_assert_true(gimp_painter_brush_geometry_options(duplicate));
  g_object_set(duplicate,"painter-legacy-brush-geometry",FALSE,nullptr);g_assert_true(gimp_painter_brush_geometry_options(options));
  gimp_paint_options_copy_props(options,duplicate,GIMP_CONTEXT_PROP_MASK_BRUSH);g_assert_true(gimp_painter_brush_geometry_options(duplicate));
  gchar*serialized=gimp_config_serialize_to_string(GIMP_CONFIG(options),nullptr);g_assert_nonnull(strstr(serialized,"(painter-legacy-brush-geometry yes)"));
  GError*error=nullptr;g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(duplicate),serialized,-1,nullptr,&error));g_assert_no_error(error);g_free(serialized);
  g_assert_true(gimp_painter_brush_geometry_options(duplicate));
  gimp_config_reset(GIMP_CONFIG(duplicate));g_assert_false(gimp_painter_brush_geometry_options(duplicate));
  g_object_unref(duplicate);g_object_unref(options);
}
static void provenance_parser () {
  const char*input="(tool-options \"GimpPaintOptions\" (tool \"gimp-paintbrush-tool\") (brush-angle 31))";GError*error=nullptr;
  auto*plain=gimp_painter_profile_transform(input,"preset",FALSE,&error);g_assert_no_error(error);g_assert_cmpstr(plain,==,input);g_free(plain);
  auto*converted=gimp_painter_profile_transform(input,"preset",TRUE,&error);g_assert_no_error(error);
  g_assert_nonnull(strstr(converted,"(painter-legacy-brush-geometry yes)"));g_assert_nonnull(strstr(converted,"(brush-angle 31)"));
  g_assert_true(strstr(converted,"painter-legacy-brush-geometry")<strstr(converted,"brush-angle"));g_free(converted);
  converted=gimp_painter_profile_transform("(painter-legacy-brush-geometry no)","tool-options/gimp-paintbrush-tool",TRUE,&error);g_assert_no_error(error);
  g_assert_nonnull(strstr(converted,"(painter-legacy-brush-geometry no)"));g_assert_null(strstr(converted,"(painter-legacy-brush-geometry yes)"));g_free(converted);
  for(const char*kind:{"tool-options/gimp-smudge-tool","tool-options/gimp-mypaint-tool"}){
    converted=gimp_painter_profile_transform("(use-texture yes)",kind,TRUE,&error);g_assert_no_error(error);g_assert_null(strstr(converted,"painter-legacy-brush-geometry"));g_free(converted);
  }
}
static void native_defaults () {
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));auto*b=brush(3);
  gimp_context_set_brush(GIMP_CONTEXT(options),b);
  g_object_set(options,"painter-legacy-brush-geometry",TRUE,"painter-legacy-native-hardness",TRUE,"painter-legacy-native-spacing",TRUE,nullptr);
  g_assert_cmpfloat(options->brush_hardness,==,1);g_assert_cmpfloat(options->brush_spacing,==,gimp_brush_get_spacing(b)/100.0);
  gimp_paint_options_set_default_brush_size(options,b);gint w,h;GError*error=nullptr;g_assert_true(gimp_painter_brush_geometry_size(b,&w,&h,&error));g_assert_no_error(error);g_assert_cmpfloat(options->brush_size,==,MAX(w,h));
  gimp_paint_options_set_default_brush_angle(options,b);gimp_paint_options_set_default_brush_aspect_ratio(options,b);gimp_paint_options_set_default_brush_hardness(options,b);
  g_assert_cmpfloat(options->brush_angle,==,0);g_assert_cmpfloat(options->brush_aspect_ratio,==,0);g_assert_cmpfloat(options->brush_hardness,==,1);
  g_object_set(options,"painter-legacy-brush-geometry",FALSE,"painter-legacy-native-hardness",TRUE,nullptr);
  g_assert_cmpfloat(options->brush_hardness,==,GIMP_BRUSH_GENERATED(b)->hardness);
  g_object_unref(options);g_object_unref(b);
}
static std::vector<guchar> mask_bytes (GimpBrushCore*core) {
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;coords.x=20.25;coords.y=20.25;
  const auto*mask=gimp_brush_core_get_brush_mask(core,&coords,GIMP_BRUSH_SOFT,.5);g_assert_nonnull(mask);
  return {gimp_temp_buf_get_data(mask),gimp_temp_buf_get_data(mask)+gimp_temp_buf_get_width(mask)*gimp_temp_buf_get_height(mask)};
}
static void cache_lifetime () {
  auto*core=GIMP_BRUSH_CORE(g_object_new(GIMP_TYPE_PAINTBRUSH,nullptr));auto*b=brush(3);auto*other=brush(4);
  gimp_brush_core_set_brush(core,b);core->brush=b;core->scale=.8;gimp_painter_brush_geometry_enable(core,TRUE);
  auto first=mask_bytes(core);g_assert(first==mask_bytes(core));
  gimp_brush_generated_set_radius(GIMP_BRUSH_GENERATED(b),12.25);auto second=mask_bytes(core);g_assert(first!=second);
  gimp_brush_core_set_brush(core,other);core->brush=other;
  gimp_brush_generated_set_radius(GIMP_BRUSH_GENERATED(b),7.25);gimp_brush_core_set_brush(core,b);core->brush=b;
  g_assert(second!=mask_bytes(core));
  const auto legacy=mask_bytes(core);gimp_painter_brush_geometry_enable(core,FALSE);const auto modern=mask_bytes(core);g_assert(legacy!=modern);
  gimp_painter_brush_geometry_enable(core,TRUE);g_assert(legacy==mask_bytes(core));
  g_object_unref(other);g_object_unref(b);g_object_unref(core);
}
static void original_default_dynamics () {
  std::istringstream input(fixture("legacy-brush-geometry","default-dynamics.tsv.gz"));std::string tag,hex;int identity,points,count;
  input>>tag>>identity>>points>>count;g_assert_cmpstr(tag.c_str(),==,"CURVE_DEFAULT");g_assert_cmpint(identity,==,0);g_assert_cmpint(points,==,17);g_assert_cmpint(count,==,256);
  std::vector<double> samples(count);int index;
  for(int i=0;i<count;++i){input>>tag>>index>>hex;g_assert_cmpstr(tag.c_str(),==,"CURVE_SAMPLE");g_assert_cmpint(index,==,i);samples[i]=g_ascii_strtod(hex.c_str(),nullptr);}
  auto*options=GIMP_PAINT_OPTIONS(g_object_new(GIMP_TYPE_PAINT_OPTIONS,"gimp",gimp,nullptr));
  auto*dynamics=GIMP_DYNAMICS(g_object_new(GIMP_TYPE_DYNAMICS,"name","default geometry dynamics",nullptr));
  g_object_set(gimp_dynamics_get_output(dynamics,GIMP_DYNAMICS_OUTPUT_FORCE),"use-pressure",TRUE,nullptr);
  GimpCoords coords=GIMP_COORDS_DEFAULT_VALUES;
  for(int i=0;i<1028;++i){
    coords.pressure=i<1024?i/1023.0:i==1024?.37:i==1025?.48:i==1026?.1:.4;
    const double modern=gimp_dynamics_get_linear_value(dynamics,GIMP_DYNAMICS_OUTPUT_FORCE,&coords,options,1);
    const double pos=coords.pressure*255;const int j=MIN(254,int(pos));const double f=pos-j;
    const double expected=(1-f)*samples[j]+f*samples[j+1];
    g_object_set(options,"painter-legacy-brush-geometry",TRUE,nullptr);
    g_assert_cmpfloat(gimp_dynamics_get_linear_value(dynamics,GIMP_DYNAMICS_OUTPUT_FORCE,&coords,options,1),==,expected);
    g_object_set(options,"painter-legacy-brush-geometry",FALSE,nullptr);
    g_assert_cmpfloat(gimp_dynamics_get_linear_value(dynamics,GIMP_DYNAMICS_OUTPUT_FORCE,&coords,options,1),==,modern);
  }
  g_object_unref(dynamics);g_object_unref(options);
}
static void invalid_transform () {
  auto*b=brush(3);GError*error=nullptr;
  for(double value:{std::numeric_limits<double>::quiet_NaN(),std::numeric_limits<double>::infinity(),-1.0,0.0,1e100}) {
    g_assert_null(gimp_painter_brush_geometry_transform(b,value,0,0,FALSE,1,FALSE,&error));g_assert_nonnull(error);g_clear_error(&error);
  }
  g_assert_null(gimp_painter_brush_geometry_transform(b,1,0,0,FALSE,2,FALSE,&error));g_assert_nonnull(error);g_clear_error(&error);g_object_unref(b);
}
int main(int argc,char**argv) {
  g_test_init(&argc,&argv,nullptr);gimp=gimp_init_for_testing();
  g_test_add_func("/painter/geometry/old-masks",original_masks);
  g_test_add_func("/painter/geometry/options",options_roundtrip);
  g_test_add_func("/painter/geometry/defaults",native_defaults);
  g_test_add_func("/painter/geometry/provenance",provenance_parser);
  g_test_add_func("/painter/geometry/cache",cache_lifetime);
  g_test_add_func("/painter/geometry/invalid",invalid_transform);
  g_test_add_func("/painter/geometry/default-dynamics",original_default_dynamics);
  return g_test_run();
}
