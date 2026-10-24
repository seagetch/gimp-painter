/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "resource.hpp"
#include "mypaintbrush-settings-data.h"
#include <json-glib/json-glib.h>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <stdexcept>
using namespace GimpPainter::MyPaint;

// Independent structural oracle: never calls Resource accessors/encode to form expected nodes.
static void same(JsonNode*a,JsonNode*b) {
  g_assert_cmpint(json_node_get_node_type(a),==,json_node_get_node_type(b));
  if(JSON_NODE_HOLDS_OBJECT(a)) {
    auto*x=json_node_get_object(a);auto*y=json_node_get_object(b);
    g_assert_cmpuint(json_object_get_size(x),==,json_object_get_size(y));
    GList*names=json_object_get_members(x);
    for(auto*p=names;p;p=p->next){auto*k=static_cast<const char*>(p->data);g_assert_true(json_object_has_member(y,k));same(json_object_get_member(x,k),json_object_get_member(y,k));}
    g_list_free(names);
  } else if(JSON_NODE_HOLDS_ARRAY(a)) {
    auto*x=json_node_get_array(a);auto*y=json_node_get_array(b);g_assert_cmpuint(json_array_get_length(x),==,json_array_get_length(y));
    for(guint i=0;i<json_array_get_length(x);++i)same(json_array_get_element(x,i),json_array_get_element(y,i));
  } else if(JSON_NODE_HOLDS_VALUE(a)) {
    GType x=json_node_get_value_type(a),y=json_node_get_value_type(b);
    if(x==G_TYPE_INT64 && y==G_TYPE_INT64)g_assert_true(json_node_get_int(a)==json_node_get_int(b));
    else if(x==G_TYPE_DOUBLE && y==G_TYPE_DOUBLE)g_assert_cmpfloat(json_node_get_double(a),==,json_node_get_double(b));
    else if((x==G_TYPE_DOUBLE&&y==G_TYPE_INT64)||(x==G_TYPE_INT64&&y==G_TYPE_DOUBLE)) {
      const double d=json_node_get_double(x==G_TYPE_DOUBLE?a:b);
      const gint64 i=json_node_get_int(x==G_TYPE_INT64?a:b);
      // Check bounds before conversion: INT64_MAX rounds UP as a double.
      g_assert_true(std::isfinite(d)&&d>=-9223372036854775808.0&&d<9223372036854775808.0);
      g_assert_true(std::trunc(d)==d);g_assert_true(static_cast<gint64>(d)==i);
    }
    else {g_assert_true(x==y);if(x==G_TYPE_STRING)g_assert_cmpstr(json_node_get_string(a),==,json_node_get_string(b));else {g_assert_true(x==G_TYPE_BOOLEAN);g_assert_cmpint(json_node_get_boolean(a),==,json_node_get_boolean(b));}}
  }
}
static JsonParser*parse(const std::string&s){auto*p=json_parser_new();GError*e=nullptr;g_assert_true(json_parser_load_from_data(p,s.data(),s.size(),&e));g_assert_no_error(e);return p;}
static void compare(JsonParser*expected,const std::string&actual){auto*p=parse(actual);same(json_parser_get_root(expected),json_parser_get_root(p));g_object_unref(p);}
static const char*fixture=R"({"version":3,"vendor":{"wide":[9007199254740993,-9007199254740993,9223372036854775807,-9223372036854775808,{"nested":9007199254740995}],"emoji":"紙🖌️","array":[null,false,42,-3.25,{"":"空","escaped":"\"\\\n"}]},"settings":{"hardness":{"base_value":0.75,"annotation":{"keep":true},"inputs":{"pressure":[[-1,-2],[0,0],[1,2]],"unrecognized":{"preserve":[3,null]}}}},"switches":{"unknown":{"yes":false}},"texts":{"brushmark_name":"存在しない筆🖌️","texture_name":"欠落紙目\"\\\n","unknown":[null,7]},"parent_brush_name":"祖先","group":"群"})";
static void lossless() {
  auto*small_int=parse("[1,-2,0]");auto*small_double=parse("[1.0,-2.0,0.0]");
  same(json_parser_get_root(small_int),json_parser_get_root(small_double));
  same(json_parser_get_root(small_double),json_parser_get_root(small_int));
  g_object_unref(small_int);g_object_unref(small_double);
  auto expected=parse(fixture);
  auto*wide=json_object_get_array_member(json_object_get_object_member(json_node_get_object(json_parser_get_root(expected)),"vendor"),"wide");
  const gint64 known[]={G_GINT64_CONSTANT(9007199254740993),-G_GINT64_CONSTANT(9007199254740993),G_MAXINT64,G_MININT64};
  for(guint i=0;i<4;++i){g_assert_true(json_node_get_value_type(json_array_get_element(wide,i))==G_TYPE_INT64);g_assert_true(json_array_get_int_element(wide,i)==known[i]);}
  g_assert_true(json_object_get_int_member(json_array_get_object_element(wide,4),"nested")==G_GINT64_CONSTANT(9007199254740995));
  auto r=Resource::decode(fixture);compare(expected,r.encode());
  auto copy=r;copy.set_base_value(BRUSH_HARDNESS,.125);copy.set_curve(BRUSH_HARDNESS,INPUT_PRESSURE,{{-2,-4},{0,.5},{3,6}});
  auto*settings=json_object_get_object_member(json_node_get_object(json_parser_get_root(expected)),"settings");auto*hard=json_object_get_object_member(settings,"hardness");json_object_set_double_member(hard,"base_value",.125);
  auto*points=parse("[[-2,-4],[0,0.5],[3,6]]");json_object_set_member(json_object_get_object_member(hard,"inputs"),"pressure",json_node_copy(json_parser_get_root(points)));g_object_unref(points);
  auto*root=json_node_get_object(json_parser_get_root(expected));
  copy.set_switch(BRUSH_NON_INCREMENTAL,true);json_object_set_boolean_member(json_object_get_object_member(root,"switches"),"non_incremental",true);
  copy.set_text(BRUSH_TEXTURE_NAME,"変更紙𠮷");json_object_set_string_member(json_object_get_object_member(root,"texts"),"texture_name","変更紙𠮷");
  copy.set_group("新群");json_object_set_string_member(root,"group","新群");copy.set_parent_brush_name("新親🖌️");json_object_set_string_member(root,"parent_brush_name","新親🖌️");
  compare(expected,copy.encode());compare(expected,Resource::decode(copy.encode()).encode());
  auto original=parse(fixture);compare(original,r.encode());g_object_unref(original);g_object_unref(expected);
}
static void defaults_and_texts(){
  unsigned scenarios=0;
  for(const auto&sw:painter_mypaint_switches)for(int state=0;state<3;++state){
    auto r=Resource::decode("{\"version\":3}");auto*p=parse("{\"version\":3}");auto*root=json_node_get_object(json_parser_get_root(p));
    if(state){r.set_switch(sw.index,state==2);auto*o=json_object_new();json_object_set_boolean_member(o,sw.internal_name,state==2);json_object_set_object_member(root,"switches",o);}
    for(int pass=0;pass<2;++pass){g_assert_cmpint(r.has_member("switches",sw.internal_name),==,state!=0);g_assert_cmpint(r.switch_value(sw.index),==,state==2);compare(p,r.encode());r=Resource::decode(r.encode());}
    g_object_unref(p);++scenarios;
  }
  for(const auto&t:painter_mypaint_texts)for(int state=0;state<4;++state){
    auto r=Resource::decode("{\"version\":3}");auto*p=parse("{\"version\":3}");const char*value=state==3?"欠落🖌️ \"\\\n":state==2?"":nullptr;
    if(state){r.set_text(t.index,value);auto*o=json_object_new();if(value)json_object_set_string_member(o,t.internal_name,value);else json_object_set_null_member(o,t.internal_name);json_object_set_object_member(json_node_get_object(json_parser_get_root(p)),"texts",o);}
    for(int pass=0;pass<2;++pass){g_assert_cmpint(r.has_member("texts",t.internal_name),==,state!=0);g_assert_cmpint(r.text_is_null(t.index),==,state<2);g_assert_cmpstr(r.peek_text(t.index),==,value);compare(p,r.encode());r=Resource::decode(r.encode());}
    g_object_unref(p);++scenarios;
  }
  g_test_message("default/null/empty/Unicode scenarios=%u",scenarios);
}
typedef struct {GOutputStream parent;GByteArray*bytes;gsize chunk,fail_after,cancel_after;unsigned calls,closed;} ProbeOutput;
typedef struct {GOutputStreamClass parent;} ProbeOutputClass;
G_DEFINE_TYPE(ProbeOutput,probe_output,G_TYPE_OUTPUT_STREAM)
static gssize output_write(GOutputStream*base,const void*buffer,gsize count,GCancellable*cancel,GError**error){
  auto*p=reinterpret_cast<ProbeOutput*>(base);++p->calls;
  if(g_cancellable_set_error_if_cancelled(cancel,error))return -1;
  if(p->bytes->len>=p->fail_after){g_set_error_literal(error,G_IO_ERROR,G_IO_ERROR_PERMISSION_DENIED,"injected write failure");return -1;}
  auto n=std::min({count,p->chunk,p->fail_after-p->bytes->len});g_byte_array_append(p->bytes,static_cast<const guint8*>(buffer),n);
  if(p->bytes->len>=p->cancel_after&&cancel)g_cancellable_cancel(cancel);return n;
}
static gboolean output_close(GOutputStream*base,GCancellable*,GError**){++reinterpret_cast<ProbeOutput*>(base)->closed;return TRUE;}
static void output_finalize(GObject*object){g_byte_array_unref(reinterpret_cast<ProbeOutput*>(object)->bytes);G_OBJECT_CLASS(probe_output_parent_class)->finalize(object);}
static void probe_output_class_init(ProbeOutputClass*c){G_OUTPUT_STREAM_CLASS(c)->write_fn=output_write;G_OUTPUT_STREAM_CLASS(c)->close_fn=output_close;G_OBJECT_CLASS(c)->finalize=output_finalize;}
static void probe_output_init(ProbeOutput*p){p->bytes=g_byte_array_new();p->chunk=7;p->fail_after=G_MAXSIZE;p->cancel_after=G_MAXSIZE;p->calls=p->closed=0;}
static void output_matrix(){
  const auto r=Resource::decode(fixture);const auto serialized=r.encode();auto*expected=parse(fixture);
  for(int mode=0;mode<5;++mode){auto*p=reinterpret_cast<ProbeOutput*>(g_object_new(probe_output_get_type(),nullptr));auto*c=g_cancellable_new();GError*e=nullptr;
    if(mode==1)p->fail_after=0;if(mode==2)p->fail_after=17;if(mode==3)g_cancellable_cancel(c);if(mode==4)p->cancel_after=14;
    bool saved=r.save(G_OUTPUT_STREAM(p),c,&e);
    if(mode==0){g_assert_true(saved);g_assert_no_error(e);g_assert_cmpuint(p->calls,>,1);compare(expected,std::string(reinterpret_cast<char*>(p->bytes->data),p->bytes->len));}
    else {g_assert_false(saved);const int expected_code=mode<3?G_IO_ERROR_PERMISSION_DENIED:G_IO_ERROR_CANCELLED;g_assert_error(e,G_IO_ERROR,expected_code);g_assert_cmpuint(p->bytes->len,==,mode==2?17:mode==4?14:0);g_assert_cmpuint(p->bytes->len,<,serialized.size());g_assert_cmpmem(p->bytes->data,p->bytes->len,serialized.data(),p->bytes->len);g_clear_error(&e);}
    g_assert_cmpuint(p->closed,==,0);g_assert_false(g_output_stream_is_closed(G_OUTPUT_STREAM(p)));g_assert_true(r.encode()==serialized);g_object_unref(c);g_object_unref(p);
  }g_object_unref(expected);
}
static void malformed(){for(const char*text:{"{\"version\":3,\"switches\":{\"non_incremental\":null}}","{\"version\":3,\"texts\":{\"brushmark_name\":false}}","{\"version\":3,\"texts\":[]}","{\"version\":3,\"switches\":[]}"}){bool rejected=false;try{(void)Resource::decode(text);}catch(const std::invalid_argument&){rejected=true;}g_assert_true(rejected);}}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);g_test_add_func("/resource-proof/lossless",lossless);g_test_add_func("/resource-proof/defaults-texts",defaults_and_texts);g_test_add_func("/resource-proof/output",output_matrix);g_test_add_func("/resource-proof/malformed",malformed);return g_test_run();}
