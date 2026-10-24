/* SPDX-License-Identifier: GPL-3.0-or-later
 * Independent original metadata oracle; Resource missing defaults stay FALSE. */
namespace SwitchDefaults {
struct Expected {const char*name;int index;gboolean value;};
static const Expected expected[] = {
  {"non-incremental",BRUSH_NON_INCREMENTAL,FALSE},
  {"use-gimp-brushmark",BRUSH_USE_GIMP_BRUSHMARK,TRUE},
  {"use-gimp-texture",BRUSH_USE_GIMP_TEXTURE,FALSE},
  {"brushmark-specified",BRUSH_BRUSHMARK_SPECIFIED,FALSE},
  {"texture-specified",BRUSH_TEXTURE_SPECIFIED,FALSE}
};
static gboolean value(GimpPainterMybrushOptions*o,const char*n)
{gboolean result=FALSE;g_object_get(o,n,&result,nullptr);return result;}
static void load(GimpPainterMybrushOptions*o,const std::string&text)
{GError*error=nullptr;g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(o),text.c_str(),-1,nullptr,&error));g_assert_no_error(error);}
static std::string changed(GimpPainterMybrushOptions*o)
{GString*s=g_string_new(nullptr);auto*w=gimp_config_writer_new_from_string(s);g_assert_true(gimp_config_serialize_changed_properties(GIMP_CONFIG(o),w));GError*error=nullptr;g_assert_true(gimp_config_writer_finish(w,nullptr,&error));g_assert_no_error(error);std::string out(s->str,s->len);g_string_free(s,TRUE);return out;}
static void metadata_reset()
{
  auto o=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());
  for(const auto&e:expected){auto*p=g_object_class_find_property(G_OBJECT_GET_CLASS(o.get()),e.name);g_assert_true(G_IS_PARAM_SPEC_BOOLEAN(p));g_assert_cmpint(G_PARAM_SPEC_BOOLEAN(p)->default_value,==,e.value);g_assert_false(p->flags&G_PARAM_CONSTRUCT);g_assert_false(value(o.get(),e.name));g_assert_false(PainterOptionsRef::retain(o.get()).snapshot().switch_value(e.index));g_object_set(o.get(),e.name,!e.value,nullptr);gimp_config_reset_property(G_OBJECT(o.get()),e.name);g_assert_cmpint(value(o.get(),e.name),==,e.value);}
}
static void explicit_and_omitted_config()
{
  for(const auto&e:expected)for(gboolean v:{0,1}){auto o=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());load(o.get(),std::string("(")+e.name+(v?" yes)":" no)"));g_assert_cmpint(value(o.get(),e.name),==,v);String text(gimp_config_serialize_to_string(GIMP_CONFIG(o.get()),nullptr));auto copy=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());load(copy.get(),text.get());g_assert_cmpint(value(copy.get(),e.name),==,v);g_assert_true(PainterOptionsRef::retain(copy.get()).snapshot().encode()==PainterOptionsRef::retain(o.get()).snapshot().encode());}
  auto fresh=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());load(fresh.get(),"(opaque 0.7)");g_assert_false(value(fresh.get(),"use-gimp-brushmark"));
  auto reset=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());gimp_config_reset(GIMP_CONFIG(reset.get()));load(reset.get(),"(opaque 0.7)");g_assert_true(value(reset.get(),"use-gimp-brushmark"));
  g_test_message("omitted switch preserves receiver: fresh Resource FALSE; reset receiver TRUE; scalar-only omission after reset intentionally now TRUE; actual native captures tested separately");
}
static void prior_native_replay()
{
  const char*directory=g_getenv("GIMP_TEST_PRIOR_SWITCH_CONFIGS");
  if(!directory){g_test_skip("requires sealed actual prechange serializer captures");return;}
  for(gboolean v:{0,1})for(const char*kind:{"full","changed"}){
    std::string path=std::string(directory)+(v?"/true-":"/false-")+kind+".rc";
    gchar*contents=nullptr;gsize length=0;GError*error=nullptr;
    g_assert_true(g_file_get_contents(path.c_str(),&contents,&length,&error));g_assert_no_error(error);
    std::string text(contents,length);g_free(contents);
    g_assert_true(text.find("painter-settings")!=std::string::npos);
    auto receiver=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());
    gimp_config_reset(GIMP_CONFIG(receiver.get()));load(receiver.get(),text);
    g_assert_cmpint(value(receiver.get(),"use-gimp-brushmark"),==,v);
    g_test_message("actual prechange native %s %s preserves value after reset",v?"TRUE":"FALSE",kind);
    if(!v){auto start=text.find("(painter-settings ");g_assert_true(start!=std::string::npos);auto end=text.find('\n',start);auto without=text;without.erase(start,end==std::string::npos?std::string::npos:end-start+1);g_assert_true(without.find("(use-gimp-brushmark ")==std::string::npos);auto control=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());gimp_config_reset(GIMP_CONFIG(control.get()));load(control.get(),without);g_assert_true(value(control.get(),"use-gimp-brushmark"));g_test_message("removing actual native painter-settings causally changes omitted FALSE to reset TRUE");}

  }
}
static void changed_serialization()
{
  for(const auto&e:expected)for(gboolean v:{0,1}){auto o=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());g_object_set(o.get(),e.name,v,nullptr);auto text=changed(o.get());const auto field=std::string("(")+e.name+" ";g_assert_cmpint(text.find(field)!=std::string::npos,==,v!=e.value);auto copy=ObjectRef<GimpPainterMybrushOptions>::adopt(options_new());load(copy.get(),text);g_assert_cmpint(value(copy.get(),e.name),==,v);g_assert_true(PainterOptionsRef::retain(copy.get()).snapshot().encode()==PainterOptionsRef::retain(o.get()).snapshot().encode());}
}
}
