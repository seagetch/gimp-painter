/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
extern "C" {
#include "libgimpconfig/gimpconfig.h"
#include "core/core-types.h"
#include "widgets/widgets-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpdatafactory.h"
#include "core/gimpbrushgenerated.h"
#include "core/gimppattern.h"
#include "core/gimppaintermybrush.h"
#include "tests.h"
#include "gimp-app-test-utils.h"
}
#include "paint/painter-mypaint-surface/gimp-painter-options.hpp"
#include "core/gimppaintermybrush-handle.hpp"
#include "painter/gimp-painter-binding.h"
#include "mypaintbrush-settings-data.h"
#include <algorithm>
using namespace GimpPainter;
using MyPaint::Resource;
static Gimp*gimp;
static Resource reset_scalar_defaults(Resource resource);
static GimpPainterMybrushOptions*options_new()
{return GIMP_PAINTER_MYBRUSH_OPTIONS(g_object_new(GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS,"gimp",gimp,nullptr));}
static std::string name(const char*n){std::string s(n);std::replace(s.begin(),s.end(),'_','-');return s;}
static void generated_properties()
{
  auto*options=options_new();g_assert_false(options->binding_failed);auto*klass=G_OBJECT_GET_CLASS(options);
  for(const auto&s:painter_mypaint_settings){auto*p=g_object_class_find_property(klass,name(s.internal_name).c_str());g_assert_true(G_IS_PARAM_SPEC_DOUBLE(p));g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(p)->minimum,==,s.minimum);g_assert_cmpfloat(G_PARAM_SPEC_DOUBLE(p)->maximum,==,s.maximum);}
  for(const auto&s:painter_mypaint_switches)g_assert_true(G_IS_PARAM_SPEC_BOOLEAN(g_object_class_find_property(klass,name(s.internal_name).c_str())));
  for(const auto&s:painter_mypaint_texts)g_assert_true(G_IS_PARAM_SPEC_STRING(g_object_class_find_property(klass,name(s.internal_name).c_str())));
  for(const char*key:{"painter-draft-name","painter-name-dirty"}) {
    auto*spec=g_object_class_find_property(klass,key);g_assert_nonnull(spec);
    g_assert_true((spec->flags&G_PARAM_READABLE)!=0);
    g_assert_true((spec->flags&(G_PARAM_WRITABLE|GIMP_CONFIG_PARAM_SERIALIZE))==0);
  }
  auto*again=gimp_painter_mybrush_options_ref_for_context(GIMP_CONTEXT(options),nullptr);g_assert_true(again==options);g_object_unref(again);
  GError*error=nullptr;g_assert_null(gimp_painter_mybrush_options_ref_for_context(nullptr,&error));g_assert_nonnull(error);g_clear_error(&error);g_object_unref(options);
}
static void all_native_properties_config()
{
  auto *options = options_new ();
  g_assert_cmpuint (g_type_parent (GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS), ==, GIMP_TYPE_PAINT_OPTIONS);
  g_assert_true (GIMP_IS_CONFIG (options));
  g_assert_true (GIMP_IS_PAINTER_MYBRUSH_OPTIONS_CLASS (G_OBJECT_GET_CLASS (options)));
  g_assert_true (GIMP_PAINTER_MYBRUSH_OPTIONS_GET_CLASS (options) ==
    GIMP_PAINTER_MYBRUSH_OPTIONS_CLASS (G_OBJECT_GET_CLASS (options)));
  GTypeQuery query;
  g_type_query (GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS, &query);
  g_assert_cmpuint (query.instance_size, ==, sizeof (GimpPainterMybrushOptions));
  g_assert_cmpuint (query.class_size, ==, sizeof (GimpPainterMybrushOptionsClass));
  g_object_set (options, "brush-view-type", GIMP_VIEW_TYPE_LIST,
                "brush-view-size", 48, nullptr);
  for (const auto *key : {"brush-view-type", "brush-view-size"})
    {
      auto *spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), key);
      g_assert_nonnull (spec);
      g_assert_cmpuint (spec->owner_type, ==, GIMP_TYPE_PAINT_OPTIONS);
    }
  for (const auto &s : painter_mypaint_settings)
    {
      const auto key = name (s.internal_name);
      const double wanted = double (s.minimum) + (double (s.maximum) - s.minimum) * .25;
      double actual = 0;
      g_object_set (options, key.c_str (), wanted, nullptr);
      g_object_get (options, key.c_str (), &actual, nullptr);
      g_assert_cmpfloat (actual, ==, wanted);
      auto *spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), key.c_str ());
      g_assert_true ((spec->flags & GIMP_CONFIG_PARAM_SERIALIZE) != 0);
      g_assert_cmpuint (spec->owner_type, ==, GIMP_TYPE_PAINTER_MYBRUSH_OPTIONS);
    }
  for (const auto &s : painter_mypaint_switches)
    {
      const auto key = name (s.internal_name);
      gboolean actual = FALSE;
      g_object_set (options, key.c_str (), TRUE, nullptr);
      g_object_get (options, key.c_str (), &actual, nullptr); g_assert_true (actual);
    }
  for (const auto &s : painter_mypaint_texts)
    {
      const auto key = name (s.internal_name), wanted = "missing-type-test-" + key;
      gchar *actual = nullptr;
      g_object_set (options, key.c_str (), wanted.c_str (), nullptr);
      g_object_get (options, key.c_str (), &actual, nullptr);
      g_assert_cmpstr (actual, ==, wanted.c_str ()); g_free (actual);
    }
  const auto before = PainterOptionsRef::retain (options).snapshot ().encode ();
  String config (gimp_config_serialize_to_string (GIMP_CONFIG (options), nullptr));
  g_assert_nonnull (config.get ());
  auto *copy = options_new ();
  GError *error = nullptr;
  g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (copy), config.get (), -1, nullptr, &error));
  g_assert_no_error (error);
  g_assert_true (PainterOptionsRef::retain (copy).snapshot ().encode () == before);
  gint view_type = -1, view_size = 0;
  g_object_get (copy, "brush-view-type", &view_type, "brush-view-size", &view_size, nullptr);
  g_assert_cmpint (view_type, ==, GIMP_VIEW_TYPE_LIST);
  g_assert_cmpint (view_size, ==, 48);
  gimp_config_reset (GIMP_CONFIG (copy));
  g_assert_true (PainterOptionsRef::retain (copy).snapshot ().encode () ==
                 reset_scalar_defaults(Resource::decode(before)).encode ());
  g_assert_true (PainterOptionsRef::retain (options).snapshot ().encode () == before);
  g_object_unref (copy); g_object_unref (options);
}
static void legacy_brush_mode_config()
{
  static const char *nicks[] = {"normal", "normal-and-erase", "lock-alpha", "colorize"};
  static const char *names[] = {"GIMP_MYPAINT_NORMAL", "GIMP_MYPAINT_NORMAL_AND_ERASE",
                                "GIMP_MYPAINT_LOCK_ALPHA", "GIMP_MYPAINT_COLORIZE"};
  auto *options = options_new ();
  auto *spec = g_object_class_find_property (G_OBJECT_GET_CLASS (options), "brush-mode");
  g_assert_nonnull (spec); g_assert_true (G_IS_PARAM_SPEC_ENUM (spec));
  g_assert_cmpstr (g_type_name (G_PARAM_SPEC_VALUE_TYPE (spec)), ==, "GimpMypaintBrushMode");
  auto *values = G_PARAM_SPEC_ENUM (spec)->enum_class;
  g_assert_cmpuint (values->n_values, ==, 4);
  const auto draft = PainterOptionsRef::retain (options).snapshot ().encode ();
  for (int mode = 0; mode < 4; ++mode)
    {
      auto *value = g_enum_get_value (values, mode);
      g_assert_nonnull (value); g_assert_cmpstr (value->value_nick, ==, nicks[mode]);
      g_assert_cmpstr (value->value_name, ==, names[mode]);
      String input (g_strdup_printf ("(brush-mode %s)", nicks[mode]));
      GError *error = nullptr;
      g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (options), input.get (), -1, nullptr, &error));
      g_assert_no_error (error);
      gint actual = -1; g_object_get (options, "brush-mode", &actual, nullptr);
      g_assert_cmpint (actual, ==, mode);
      g_assert_true (PainterOptionsRef::retain (options).snapshot ().encode () == draft);
      String saved (gimp_config_serialize_to_string (GIMP_CONFIG (options), nullptr));
      if (mode == 0)
        g_assert_null (strstr (saved.get (), "(brush-mode "));
      else
        g_assert_nonnull (strstr (saved.get (), input.get ()));
      auto *copy = options_new ();
      g_assert_true (gimp_config_deserialize_string (GIMP_CONFIG (copy), saved.get (), -1, nullptr, &error));
      g_assert_no_error (error);
      g_object_get (copy, "brush-mode", &actual, nullptr); g_assert_cmpint (actual, ==, mode);
      auto *duplicate = GIMP_PAINTER_MYBRUSH_OPTIONS (gimp_config_duplicate (GIMP_CONFIG (options)));
      g_assert_nonnull (duplicate);
      g_object_get (duplicate, "brush-mode", &actual, nullptr); g_assert_cmpint (actual, ==, mode);
      gimp_config_reset (GIMP_CONFIG (copy));
      g_object_set (copy, "brush-mode", (mode + 1) % 4, nullptr);
      g_assert_true (gimp_config_copy (GIMP_CONFIG (options), GIMP_CONFIG (copy), GParamFlags (0)));
      g_object_get (copy, "brush-mode", &actual, nullptr); g_assert_cmpint (actual, ==, mode);
      gimp_config_reset (GIMP_CONFIG (copy));
      g_object_get (copy, "brush-mode", &actual, nullptr); g_assert_cmpint (actual, ==, 0);
      g_object_unref (copy); g_object_unref (duplicate);
    }
  GError *error = nullptr;
  g_assert_false (gimp_config_deserialize_string (GIMP_CONFIG (options),
    "(brush-mode future-mode)", -1, nullptr, &error));
  g_assert_nonnull (error); g_clear_error (&error);
  gint actual = -1; g_object_get (options, "brush-mode", &actual, nullptr);
  g_assert_cmpint (actual, ==, 3);
  g_object_unref (options);
}
static void edit_curve_commit_and_conflict()
{
  auto*options=options_new();auto brush=PainterMybrushRef::create("draft-source");
  auto initial=Resource::decode("{\"version\":3,\"settings\":{},\"future_metadata\":{\"keep\":true}}");brush.replace(initial);gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),brush.get());
  g_object_set(options,"stroke-opacity",.37,"non-incremental",TRUE,"brushmark-name","missing-shape-kept",nullptr);
  const GimpVector2 points[]={{0,0},{.5,.7},{1,1}};GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_curve(options,BRUSH_OPAQUE_MULTIPLY,INPUT_PRESSURE,points,3,&error));g_assert_no_error(error);
  auto draft=PainterOptionsRef::retain(options).snapshot();g_assert_true(draft.switch_value(BRUSH_NON_INCREMENTAL));g_assert_cmpfloat_with_epsilon(draft.base_value(BRUSH_STROKE_OPACITY),.37,1e-6);
  g_assert_true(draft.text_value(BRUSH_BRUSHMARK_NAME)=="missing-shape-kept");g_assert_cmpuint(draft.curve(BRUSH_OPAQUE_MULTIPLY,INPUT_PRESSURE).size(),==,3);
  g_assert_true(brush.snapshot().encode()==initial.encode());g_assert_true(gimp_painter_mybrush_options_commit(options,&error));g_assert_no_error(error);
  g_assert_true(brush.snapshot().encode()==draft.encode());gboolean dirty,conflict;g_object_get(options,"painter-dirty",&dirty,"painter-conflict",&conflict,nullptr);g_assert_false(dirty);g_assert_false(conflict);
  g_object_set(options,"stroke-opacity",.6,nullptr);auto retained=PainterOptionsRef::retain(options).snapshot().encode();
  auto external=brush.snapshot();external.set_base_value(BRUSH_STROKE_OPACITY,.8);brush.replace(external);
  g_object_get(options,"painter-conflict",&conflict,nullptr);g_assert_true(conflict);g_assert_false(gimp_painter_mybrush_options_commit(options,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==retained);g_assert_true(brush.snapshot().encode()==external.encode());g_object_unref(options);
}
static void history_and_config_roundtrip()
{
  auto*options=options_new();auto a=PainterMybrushRef::create("history-a"),b=PainterMybrushRef::create("history-b");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),a.get());GError*error=nullptr;
  const char*json="{\"version\":3,\"settings\":{\"opaque\":{\"base_value\":0.7,\"inputs\":{\"pressure\":[[0,0],[1,0.4]]}}},\"switches\":{\"non_incremental\":true},\"texts\":{\"texture_name\":\"absent-paper\"},\"unknown\":{\"remember\":23}}";
  g_assert_true(gimp_painter_mybrush_options_set_json(options,json,&error));g_assert_no_error(error);auto before=PainterOptionsRef::retain(options).snapshot().encode();
  const guint start=gimp_painter_mybrush_options_history_size(options);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),b.get());g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,start+1);
  g_assert_true(gimp_painter_mybrush_options_restore_history(options,start,&error));g_assert_no_error(error);g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==before);
  String config(gimp_config_serialize_to_string(GIMP_CONFIG(options),nullptr));g_assert_nonnull(config.get());auto*copy=options_new();
  g_assert_true(gimp_config_deserialize_string(GIMP_CONFIG(copy),config.get(),-1,nullptr,&error));g_assert_no_error(error);g_assert_true(PainterOptionsRef::retain(copy).snapshot().encode()==before);
  auto*duplicate=GIMP_PAINTER_MYBRUSH_OPTIONS(gimp_config_duplicate(GIMP_CONFIG(options)));g_assert_nonnull(duplicate);g_assert_true(PainterOptionsRef::retain(duplicate).snapshot().encode()==before);
  g_assert_true(gimp_config_copy(GIMP_CONFIG(options),GIMP_CONFIG(copy),GParamFlags(0)));g_assert_true(PainterOptionsRef::retain(copy).snapshot().encode()==before);
  g_object_unref(duplicate);g_object_unref(copy);g_object_unref(options);
}
static void assert_draft_name(GimpPainterMybrushOptions*options,const char*expected,gboolean name_dirty)
{
  GError*error=nullptr;
  String actual(gimp_painter_mybrush_options_dup_draft_name(options,&error));
  g_assert_no_error(error);g_assert_cmpstr(actual.get(),==,expected);
  gchar*property=nullptr;gboolean dirty=FALSE;
  g_object_get(options,"painter-draft-name",&property,"painter-name-dirty",&dirty,nullptr);
  g_assert_cmpstr(property,==,expected);g_assert_cmpint(dirty,==,name_dirty);g_free(property);
}
static void draft_name_identity_and_commit()
{
  auto*options=options_new();auto source=PainterMybrushRef::create("name-source");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),source.get());
  const auto source_before=source.snapshot().encode();
  assert_draft_name(options,"name-source",FALSE);
  guint settings_notifications=0,name_notifications=0;
  auto count=+[](GimpPainterMybrushOptions*,gpointer data){++*static_cast<guint*>(data);};
  g_signal_connect(options,"settings-changed",G_CALLBACK(count),&settings_notifications);
  g_signal_connect(options,"notify::painter-draft-name",G_CALLBACK(+[](GObject*,GParamSpec*,gpointer data){++*static_cast<guint*>(data);}),&name_notifications);
  GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Pending \xcf\x80 brush",&error));
  g_assert_no_error(error);assert_draft_name(options,"Pending \xcf\x80 brush",TRUE);
  g_assert_cmpuint(settings_notifications,==,0);g_assert_cmpuint(name_notifications,==,1);
  gboolean dirty=TRUE;g_object_get(options,"painter-dirty",&dirty,nullptr);g_assert_false(dirty);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==source.get());
  g_assert_cmpstr(gimp_object_get_name(source.get()),==,"name-source");
  g_assert_true(source.snapshot().encode()==source_before);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Pending \xcf\x80 brush",&error));
  g_assert_no_error(error);g_assert_cmpuint(name_notifications,==,1);
  g_object_set(options,"opaque",.341,nullptr);
  const auto edited=PainterOptionsRef::retain(options).snapshot().encode();
  assert_draft_name(options,"Pending \xcf\x80 brush",TRUE);
  g_assert_true(source.snapshot().encode()==source_before);
  g_assert_true(gimp_painter_mybrush_options_commit(options,&error));g_assert_no_error(error);
  g_assert_true(source.snapshot().encode()==edited);assert_draft_name(options,"Pending \xcf\x80 brush",TRUE);
  g_object_get(options,"painter-dirty",&dirty,nullptr);g_assert_false(dirty);
  g_assert_cmpstr(gimp_object_get_name(source.get()),==,"name-source");
  const guint notified=settings_notifications;
  gimp_object_set_name(GIMP_OBJECT(source.get()),"explicit-resource-rename");
  assert_draft_name(options,"Pending \xcf\x80 brush",TRUE);
  g_assert_cmpuint(settings_notifications,==,notified);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"explicit-resource-rename",&error));
  g_assert_no_error(error);assert_draft_name(options,"explicit-resource-rename",FALSE);
  gimp_object_set_name(GIMP_OBJECT(source.get()),"clean-name-follows-source");
  assert_draft_name(options,"clean-name-follows-source",FALSE);
  g_assert_cmpuint(settings_notifications,==,notified);g_object_unref(options);
}
static void draft_name_history_and_copy()
{
  auto*options=options_new();auto*other=options_new();
  auto a=PainterMybrushRef::create("named-history-source"),b=PainterMybrushRef::create("named-history-next");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),a.get());
  const auto source_before=a.snapshot().encode();
  const auto original=Resource::decode("{\"version\":3,\"unknown\":{\"named_history\":true},\"settings\":{\"opaque\":{\"base_value\":0.27,\"inputs\":{\"pressure\":[[0,0.1],[1,0.7]]}}}}");
  GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_json(options,original.encode().c_str(),&error));
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"History draft name",&error));g_assert_no_error(error);
  const guint index=gimp_painter_mybrush_options_history_size(options);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),b.get());
  String label(gimp_painter_mybrush_options_history_name(options,index));g_assert_cmpstr(label.get(),==,"History draft name");
  assert_draft_name(options,"named-history-next",FALSE);
  g_assert_true(gimp_painter_mybrush_options_restore_history(other,index,&error));g_assert_no_error(error);
  assert_draft_name(other,"History draft name",TRUE);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(other))==a.get());
  g_assert_true(PainterOptionsRef::retain(other).snapshot().encode()==original.encode());
  g_assert_cmpstr(gimp_object_get_name(a.get()),==,"named-history-source");g_assert_true(a.snapshot().encode()==source_before);
  String serialized(gimp_config_serialize_to_string(GIMP_CONFIG(other),nullptr));
  g_assert_null(strstr(serialized.get(),"painter-draft-name"));g_assert_null(strstr(serialized.get(),"History draft name"));
  auto*duplicate=GIMP_PAINTER_MYBRUSH_OPTIONS(gimp_config_duplicate(GIMP_CONFIG(other)));g_assert_nonnull(duplicate);
  assert_draft_name(duplicate,"History draft name",TRUE);
  g_assert_true(gimp_config_copy(GIMP_CONFIG(other),GIMP_CONFIG(options),GParamFlags(GIMP_CONFIG_PARAM_SERIALIZE)));
  assert_draft_name(options,"History draft name",TRUE);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Independent copied name",&error));
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(duplicate,"Independent duplicate name",&error));g_assert_no_error(error);
  assert_draft_name(other,"History draft name",TRUE);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==original.encode());
  g_object_unref(duplicate);g_object_unref(other);g_object_unref(options);

  options=options_new();gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),a.get());
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Name-only history",&error));g_assert_no_error(error);
  const guint name_index=gimp_painter_mybrush_options_history_size(options);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),b.get());
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,name_index+1);
  g_assert_true(gimp_painter_mybrush_options_restore_history(options,name_index,&error));g_assert_no_error(error);
  assert_draft_name(options,"Name-only history",TRUE);
  gboolean dirty=TRUE;g_object_get(options,"painter-dirty",&dirty,nullptr);g_assert_false(dirty);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==source_before);
  guint settings_notifications=0;
  g_signal_connect(options,"settings-changed",G_CALLBACK(+[](GimpPainterMybrushOptions*,gpointer p){++*static_cast<guint*>(p);}),&settings_notifications);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Another pending name",&error));g_assert_no_error(error);
  g_assert_true(gimp_painter_mybrush_options_restore_history(options,name_index,&error));g_assert_no_error(error);
  assert_draft_name(options,"Name-only history",TRUE);g_assert_cmpuint(settings_notifications,==,0);
  g_object_unref(options);
}
static void draft_name_external_settings_and_errors()
{
  auto*options=options_new();auto source=PainterMybrushRef::create("name-error-source");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),source.get());GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Preserved pending name",&error));g_assert_no_error(error);
  auto external=source.snapshot();external.set_base_value(BRUSH_OPAQUE,.173);source.replace(external);
  assert_draft_name(options,"Preserved pending name",TRUE);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==external.encode());
  gboolean dirty=TRUE;g_object_get(options,"painter-dirty",&dirty,nullptr);g_assert_false(dirty);
  g_object_set(options,"opaque",.319,nullptr);
  const auto retained=PainterOptionsRef::retain(options).snapshot().encode();
  external.set_base_value(BRUSH_OPAQUE,.613);source.replace(external);
  g_assert_false(gimp_painter_mybrush_options_commit(options,&error));g_assert_nonnull(error);g_clear_error(&error);
  assert_draft_name(options,"Preserved pending name",TRUE);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==retained);
  g_assert_true(source.snapshot().encode()==external.encode());
  for(const char*invalid:{static_cast<const char*>(nullptr),"\xff"}) {
    g_assert_false(gimp_painter_mybrush_options_set_draft_name(options,invalid,&error));g_assert_nonnull(error);g_clear_error(&error);
    assert_draft_name(options,"Preserved pending name",TRUE);
  }
  String owned(gimp_painter_mybrush_options_dup_draft_name(options,&error));g_assert_no_error(error);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"",&error));g_assert_no_error(error);assert_draft_name(options,"",TRUE);
  g_assert_cmpstr(owned.get(),==,"Preserved pending name");
  g_assert_true(gimp_painter_binding_close(G_OBJECT(options),&error));g_assert_no_error(error);
  g_assert_null(gimp_painter_mybrush_options_dup_draft_name(options,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_assert_false(gimp_painter_mybrush_options_set_draft_name(options,"Closed",&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_assert_cmpstr(owned.get(),==,"Preserved pending name");g_object_unref(options);
}
static Resource preset_with_icon(const char *json, guint32 rgba)
{
  auto resource=Resource::decode(json);
  auto *icon=gdk_pixbuf_new(GDK_COLORSPACE_RGB,TRUE,8,3,5);
  gdk_pixbuf_fill(icon,rgba);
  gchar *png=nullptr;gsize size=0;GError *error=nullptr;
  g_assert_true(gdk_pixbuf_save_to_buffer(icon,&png,&size,"png",&error,nullptr));
  g_assert_no_error(error);
  String encoded(g_base64_encode(reinterpret_cast<const guchar*>(png),size));
  resource.set_preview_png_base64(encoded.get());
  g_free(png);g_object_unref(icon);
  return resource;
}
static Resource reset_scalar_defaults(Resource resource)
{
  // Native Reset skips properties already equal to their pspec defaults. Keep
  // untouched sparse fields absent rather than materializing new JSON members.
  for(const auto &setting:painter_mypaint_settings)
    if(resource.base_value(setting.index)!=setting.default_value)
      resource.set_base_value(setting.index,setting.default_value);
  for(const auto &setting:painter_mypaint_switches)
    if(resource.switch_value(setting.index))
      resource.set_switch(setting.index,false);
  for(const auto &setting:painter_mypaint_texts)
    if(!resource.text_is_null(setting.index))
      resource.set_text(setting.index,nullptr);
  return resource;
}
static void native_config_reset_defaults()
{
  auto *options=options_new();auto brush=PainterMybrushRef::create("reset-source");
  const auto source=preset_with_icon(
    "{\"version\":3,\"group\":\"source-group\",\"unknown\":{\"source\":17},"
    "\"settings\":{\"opaque\":{\"base_value\":0.23,\"inputs\":{\"pressure\":[[0,0.1],[1,0.9]]}},"
    "\"radius_logarithmic\":{\"base_value\":1.7}}}",0xa04080ff);
  brush.replace(source);gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),brush.get());
  const auto draft=preset_with_icon(
    "{\"version\":3,\"group\":\"draft-group\",\"unknown\":{\"retain_after_reset\":23},"
    "\"settings\":{\"opaque\":{\"base_value\":0.17,\"inputs\":{\"pressure\":[[0,0.2],[1,0.4]]}},"
    "\"radius_logarithmic\":{\"base_value\":2.1}},"
    "\"switches\":{\"non_incremental\":true},\"texts\":{\"texture_name\":\"draft-paper\"}}",0x30b070ff);
  g_assert_true(source.preview_png_base64()!=draft.preview_png_base64());
  GError *error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_json(options,draft.encode().c_str(),&error));
  g_assert_no_error(error);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Reset keeps this draft name",&error));
  g_assert_no_error(error);
  const auto expected=reset_scalar_defaults(draft);
  const guint history=gimp_painter_mybrush_options_history_size(options);
  // Reset applies legacy property defaults, but does not replace the owned preset.
  gimp_config_reset(GIMP_CONFIG(options));
  assert_draft_name(options,"Reset keeps this draft name",TRUE);
  auto actual=PainterOptionsRef::retain(options).snapshot();
  for(const auto &setting:painter_mypaint_settings)
    g_assert_cmpfloat(actual.base_value(setting.index),==,setting.default_value);
  g_assert_false(actual.switch_value(BRUSH_NON_INCREMENTAL));
  g_assert_true(actual.text_is_null(BRUSH_TEXTURE_NAME));
  g_assert_true(brush.snapshot().encode()==source.encode());
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==brush.get());
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,history);
  // Keep these explicit: the old JSON pspec default silently destroys this data.
  g_assert_true(actual.preview_png_base64()==draft.preview_png_base64());
  g_assert_true(actual.group()=="draft-group");
  g_assert_true(actual.has_member("unknown","retain_after_reset"));
  const auto curve=actual.curve(BRUSH_OPAQUE,INPUT_PRESSURE);
  g_assert_cmpuint(curve.size(),==,2);
  g_assert_cmpfloat(curve[0].y,==,.2);g_assert_cmpfloat(curve[1].y,==,.4);
  g_assert_true(actual.encode()==expected.encode());
  gimp_config_reset(GIMP_CONFIG(options));
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==expected.encode());
  assert_draft_name(options,"Reset keeps this draft name",TRUE);
  auto *pspec=g_object_class_find_property(G_OBJECT_GET_CLASS(options),"painter-settings");
  g_assert_null(g_value_get_string(g_param_spec_get_default_value(pspec)));
  const guint before_null_history=gimp_painter_mybrush_options_history_size(options);
  guint settings_notifications=0;
  g_signal_connect(options,"settings-changed",G_CALLBACK(+[](GimpPainterMybrushOptions*,gpointer p){++*static_cast<guint*>(p);}),&settings_notifications);
  g_object_set(options,"painter-settings",nullptr,nullptr);
  g_assert_cmpuint(settings_notifications,==,0);
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,before_null_history);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==expected.encode());
  assert_draft_name(options,"Reset keeps this draft name",TRUE);
  // Invalid public API input is not the property-reset sentinel.
  g_assert_false(gimp_painter_mybrush_options_set_json(options,nullptr,&error));
  g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==expected.encode());
  // An explicit valid replacement can intentionally remove the draft icon.
  const auto replacement=Resource::decode("{\"version\":3,\"group\":\"explicit-iconless\",\"settings\":{}}");
  g_assert_true(gimp_painter_mybrush_options_set_json(options,replacement.encode().c_str(),&error));
  g_assert_no_error(error);actual=PainterOptionsRef::retain(options).snapshot();
  g_assert_true(actual.encode()==replacement.encode());g_assert_true(actual.preview_png_base64().empty());
  assert_draft_name(options,"Reset keeps this draft name",TRUE);
  g_assert_true(brush.snapshot().encode()==source.encode());
  g_object_unref(options);
}
static void drop_options(GObject*,GParamSpec*,gpointer data){auto**owner=static_cast<GimpPainterMybrushOptions**>(data);g_clear_object(owner);}
static void gone(gpointer data,GObject*){*static_cast<bool*>(data)=true;}
static void notification_releases_caller()
{
  auto*options=options_new();bool finalized=false;g_object_weak_ref(G_OBJECT(options),gone,&finalized);
  g_signal_connect(options,"notify::opaque",G_CALLBACK(drop_options),&options);GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_json(options,"{\"version\":3,\"settings\":{}}",&error));g_assert_no_error(error);g_assert_null(options);g_assert_true(finalized);
  // The restore helper owns its caller through all notifications. The outer
  // native Reset operation is deliberately finished before dropping this owner.
  options=options_new();gimp_config_reset(GIMP_CONFIG(options));finalized=false;
  g_object_weak_ref(G_OBJECT(options),gone,&finalized);
  g_signal_connect(options,"notify::opaque",G_CALLBACK(drop_options),&options);
  g_assert_true(gimp_painter_mybrush_options_prepare_restore(options,&error));
  g_assert_no_error(error);g_assert_null(options);g_assert_true(finalized);
}
static void draft_name_notification_lifetime()
{
  auto*options=options_new();bool finalized=false;g_object_weak_ref(G_OBJECT(options),gone,&finalized);
  g_signal_connect(options,"notify::painter-draft-name",G_CALLBACK(drop_options),&options);GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Release the caller",&error));
  g_assert_no_error(error);g_assert_null(options);g_assert_true(finalized);

  options=options_new();auto source=PainterMybrushRef::create("rename-release-source");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),source.get());finalized=false;
  g_object_weak_ref(G_OBJECT(options),gone,&finalized);
  g_signal_connect(options,"notify::painter-draft-name",G_CALLBACK(drop_options),&options);
  gimp_object_set_name(GIMP_OBJECT(source.get()),"Rename releases the caller");
  g_assert_null(options);g_assert_true(finalized);

  options=options_new();guint dirty_notifications=0;
  g_signal_connect(options,"notify::painter-draft-name",G_CALLBACK(+[](GObject*object,GParamSpec*,gpointer){gimp_painter_binding_close(object,nullptr);}),nullptr);
  g_signal_connect(options,"notify::painter-name-dirty",G_CALLBACK(+[](GObject*,GParamSpec*,gpointer p){++*static_cast<guint*>(p);}),&dirty_notifications);
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Close during name notification",&error));g_assert_no_error(error);
  g_assert_cmpuint(dirty_notifications,==,0);
  g_assert_null(gimp_painter_mybrush_options_dup_draft_name(options,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_object_unref(options);
}
static void draft_name_reentry()
{
  auto*options=options_new();auto a=PainterMybrushRef::create("name-reentry-a"),b=PainterMybrushRef::create("name-reentry-b");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),a.get());
  struct Select {GimpPainterMybrush*next;bool fired=false;} state{b.get()};
  auto id=g_signal_connect(options,"notify::painter-draft-name",G_CALLBACK(+[](GObject*object,GParamSpec*,gpointer p){
    auto&s=*static_cast<Select*>(p);if(s.fired)return;s.fired=true;
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(object),s.next);
  }),&state);
  const guint index=gimp_painter_mybrush_options_history_size(options);GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_set_draft_name(options,"Saved before nested selection",&error));g_assert_no_error(error);
  g_assert_true(state.fired);assert_draft_name(options,"name-reentry-b",FALSE);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==b.get());
  String label(gimp_painter_mybrush_options_history_name(options,index));g_assert_cmpstr(label.get(),==,"Saved before nested selection");
  g_signal_handler_disconnect(options,id);
  gimp_object_set_name(GIMP_OBJECT(a.get()),"Detached source rename");assert_draft_name(options,"name-reentry-b",FALSE);

  guint settings_notifications=0;bool name_edited=false;
  g_signal_connect(options,"settings-changed",G_CALLBACK(+[](GimpPainterMybrushOptions*,gpointer p){++*static_cast<guint*>(p);}),&settings_notifications);
  id=g_signal_connect(options,"notify::opaque",G_CALLBACK(+[](GObject*object,GParamSpec*,gpointer p){
    auto&edited=*static_cast<bool*>(p);if(edited)return;edited=true;
    g_assert_true(gimp_painter_mybrush_options_set_draft_name(GIMP_PAINTER_MYBRUSH_OPTIONS(object),"Name edited inside settings notification",nullptr));
  }),&name_edited);
  g_assert_true(gimp_painter_mybrush_options_set_json(options,"{\"version\":3,\"settings\":{\"opaque\":{\"base_value\":0.248}}}",&error));g_assert_no_error(error);
  g_assert_true(name_edited);g_assert_cmpuint(settings_notifications,==,1);
  assert_draft_name(options,"Name edited inside settings notification",TRUE);g_signal_handler_disconnect(options,id);
  const auto expected=PainterOptionsRef::retain(options).snapshot().encode();
  id=g_signal_connect(b.get(),"settings-changed",G_CALLBACK(+[](GimpPainterMybrush*,gpointer p){
    g_assert_true(gimp_painter_mybrush_options_set_draft_name(static_cast<GimpPainterMybrushOptions*>(p),"Name edited during commit",nullptr));
  }),options);
  g_assert_true(gimp_painter_mybrush_options_commit(options,&error));g_assert_no_error(error);g_signal_handler_disconnect(b.get(),id);
  assert_draft_name(options,"Name edited during commit",TRUE);
  gboolean dirty=TRUE;g_object_get(options,"painter-dirty",&dirty,nullptr);g_assert_false(dirty);
  g_assert_true(b.snapshot().encode()==expected);g_assert_cmpstr(gimp_object_get_name(b.get()),==,"name-reentry-b");
  g_assert_cmpuint(settings_notifications,==,1);g_object_unref(options);
}
static void closed_and_invalid_curves()
{
  auto*options=options_new();const auto before=PainterOptionsRef::retain(options).snapshot().encode();GError*error=nullptr;
  const GimpVector2 invalid[]={{1,0},{0,1}};g_assert_false(gimp_painter_mybrush_options_set_curve(options,BRUSH_OPAQUE,INPUT_PRESSURE,invalid,2,&error));g_assert_nonnull(error);g_clear_error(&error);
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==before);g_assert_true(gimp_painter_binding_close(G_OBJECT(options),&error));g_assert_no_error(error);
  g_assert_null(gimp_painter_mybrush_options_dup_json(options,&error));g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(options),==,0);
  g_assert_null(gimp_painter_mybrush_options_history_name(options,0));
  g_assert_false(gimp_painter_mybrush_options_restore_history(options,0,&error));
  g_assert_error(error,GIMP_PAINTER_ERROR,GIMP_PAINTER_ERROR_CLOSED);g_clear_error(&error);g_object_unref(options);
}
static void named_resources()
{
  auto*options=options_new();auto*context=GIMP_CONTEXT(options);
  auto*brush=GIMP_BRUSH(gimp_brush_generated_new("options-shape",GIMP_BRUSH_GENERATED_CIRCLE,5,2,.7,1,0));
  auto*other=GIMP_BRUSH(gimp_brush_generated_new("options-other",GIMP_BRUSH_GENERATED_CIRCLE,3,2,.5,1,0));
  auto*pattern=GIMP_PATTERN(g_object_new(GIMP_TYPE_PATTERN,"name","options-paper",nullptr));
  auto*brushes=gimp_data_factory_get_container(gimp->brush_factory);auto*patterns=gimp_data_factory_get_container(gimp->pattern_factory);
  gimp_data_make_internal(GIMP_DATA(brush),"options-shape");gimp_data_make_internal(GIMP_DATA(other),"options-other");gimp_data_make_internal(GIMP_DATA(pattern),"options-paper");
  gimp_container_add(brushes,GIMP_OBJECT(brush));gimp_container_add(brushes,GIMP_OBJECT(other));gimp_container_add(patterns,GIMP_OBJECT(pattern));
  gimp_context_set_brush(context,brush);gimp_context_set_pattern(context,pattern);
  g_object_set(options,"use-gimp-brushmark",TRUE,"brushmark-specified",TRUE,"use-gimp-texture",TRUE,"texture-specified",TRUE,nullptr);
  auto draft=PainterOptionsRef::retain(options).snapshot();g_assert_true(draft.text_value(BRUSH_BRUSHMARK_NAME)=="options-shape");g_assert_true(draft.text_value(BRUSH_TEXTURE_NAME)=="options-paper");
  g_object_set(options,"brushmark-name","options-other",nullptr);g_assert_true(gimp_context_get_brush(context)==other);
  gimp_context_set_brush(context,brush);g_assert_true(PainterOptionsRef::retain(options).snapshot().text_value(BRUSH_BRUSHMARK_NAME)=="options-shape");
  g_object_set(options,"brushmark-name","absent-shape","texture-name","absent-paper",nullptr);
  g_assert_true(gimp_context_get_brush(context)==brush);g_assert_true(gimp_context_get_pattern(context)==pattern);
  draft=PainterOptionsRef::retain(options).snapshot();g_assert_true(draft.text_value(BRUSH_BRUSHMARK_NAME)=="absent-shape");g_assert_true(draft.text_value(BRUSH_TEXTURE_NAME)=="absent-paper");
  g_object_set(options,"brushmark-specified",FALSE,nullptr);gimp_context_set_brush(context,other);g_assert_true(PainterOptionsRef::retain(options).snapshot().text_value(BRUSH_BRUSHMARK_NAME)=="absent-shape");
  g_object_unref(options);gimp_container_remove(brushes,GIMP_OBJECT(brush));gimp_container_remove(brushes,GIMP_OBJECT(other));gimp_container_remove(patterns,GIMP_OBJECT(pattern));
  g_object_unref(brush);g_object_unref(other);g_object_unref(pattern);
}
struct Reenter {GimpPainterMybrushOptions*options;GimpPainterMybrush*next;bool fired=false;};
static void choose_again(gpointer data,GObject*){auto&s=*static_cast<Reenter*>(data);s.fired=true;gimp_context_set_painter_mybrush(GIMP_CONTEXT(s.options),s.next);}
static void selection_reentry()
{
  auto*options=options_new();auto first=PainterMybrushRef::create("first"),last=PainterMybrushRef::create("last");
  auto*old=GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_new(nullptr,"old"));
  auto value=last.snapshot();value.set_base_value(BRUSH_STROKE_OPACITY,.234);last.replace(value);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),old);Reenter state{options,last.get()};g_object_weak_ref(G_OBJECT(old),choose_again,&state);g_object_unref(old);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),first.get());g_assert_true(state.fired);g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==last.get());
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==value.encode());g_object_unref(options);

  options=options_new();gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),first.get());
  gimp_config_reset(GIMP_CONFIG(options));
  struct RestoreSelect {GimpPainterMybrush*next;bool fired=false;} restore{last.get()};
  g_signal_connect(options,"notify::opaque",G_CALLBACK(+[](GObject*object,GParamSpec*,gpointer data){
    auto &state=*static_cast<RestoreSelect*>(data);if(state.fired)return;state.fired=true;
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(object),state.next);
  }),&restore);
  GError *error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_prepare_restore(options,&error));g_assert_no_error(error);
  g_assert_true(restore.fired);g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(options))==last.get());
  g_assert_true(PainterOptionsRef::retain(options).snapshot().encode()==value.encode());g_object_unref(options);
}
static void close_during_commit(GimpPainterMybrush*,gpointer data){gimp_painter_binding_close(G_OBJECT(data),nullptr);}
static void commit_closes_options()
{
  auto*options=options_new();auto source=PainterMybrushRef::create("close-commit");gimp_context_set_painter_mybrush(GIMP_CONTEXT(options),source.get());g_object_set(options,"opaque",.42,nullptr);
  const auto expected=PainterOptionsRef::retain(options).snapshot().encode();auto id=g_signal_connect(source.get(),"settings-changed",G_CALLBACK(close_during_commit),options);GError*error=nullptr;
  g_assert_true(gimp_painter_mybrush_options_commit(options,&error));g_assert_no_error(error);g_assert_true(source.snapshot().encode()==expected);g_signal_handler_disconnect(source.get(),id);g_object_unref(options);
}
static void shared_application_history()
{
  auto*first=options_new();auto*second=options_new();
  auto a=PainterMybrushRef::create("cross-options-source"),b=PainterMybrushRef::create("cross-options-next");
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),a.get());
  const auto draft=Resource::decode("{\"version\":3,\"unknown\":{\"cross_options\":true},\"settings\":{\"texture_grain\":{\"base_value\":0,\"inputs\":{\"custom\":[[-1,0],[0,0.5],[1,1]]}}},\"texts\":{\"texture_name\":\"missing cross-options paper\"}}");
  g_assert_true(gimp_painter_mybrush_options_set_json(first,draft.encode().c_str(),nullptr));
  const auto before=gimp_painter_mybrush_options_history_size(second);
  gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),b.get());
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(second),==,before+1);
  guint notifications=0;
  g_signal_connect(second,"history-changed",G_CALLBACK(+[](GimpPainterMybrushOptions*,gpointer p){++*static_cast<guint*>(p);}),&notifications);
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  g_assert_cmpuint(notifications,>,0);
  g_object_unref(first); // history is application-owned, not first-view-owned
  GError*error=nullptr;g_assert_true(gimp_painter_mybrush_options_restore_history(second,before,&error));g_assert_no_error(error);
  g_assert_true(gimp_context_get_painter_mybrush(GIMP_CONTEXT(second))==a.get());
  g_assert_true(PainterOptionsRef::retain(second).snapshot().encode()==draft.encode());
  auto*ordinary=gimp_context_new(gimp,"cross-options-context",nullptr);
  gimp_context_set_painter_mybrush(ordinary,b.get());
  auto*third=options_new();gimp_context_set_parent(GIMP_CONTEXT(third),ordinary);
  g_assert_cmpuint(gimp_painter_mybrush_options_history_size(third),==,gimp_painter_mybrush_options_history_size(second));
  g_assert_true(gimp_painter_mybrush_options_restore_history(third,before,&error));g_assert_no_error(error);
  g_assert_true(PainterOptionsRef::retain(third).snapshot().encode()==draft.encode());
  g_object_unref(third);g_object_unref(ordinary);g_object_unref(second);
}
static void shared_history_application_close()
{
  auto*first=options_new();auto*second=options_new();
  GObject*weak=nullptr;
  {
    auto source=PainterMybrushRef::create("history-close-owned");weak=G_OBJECT(source.get());
    g_object_add_weak_pointer(weak,reinterpret_cast<gpointer*>(&weak));
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),source.get());g_object_set(first,"opaque",.147,nullptr);
    gimp_context_set_painter_mybrush(GIMP_CONTEXT(first),GIMP_PAINTER_MYBRUSH(gimp_painter_mybrush_get_standard(GIMP_CONTEXT(first))));
  }
  g_object_unref(first);g_assert_nonnull(weak);
  const auto count=gimp_painter_mybrush_options_history_size(second);g_assert_cmpuint(count,>,0);
  g_assert_true(gimp_painter_binding_close(G_OBJECT(gimp),nullptr));
  g_assert_null(weak);g_assert_cmpuint(gimp_painter_mybrush_options_history_size(second),==,0);
  while(g_main_context_pending(nullptr))g_main_context_iteration(nullptr,FALSE);
  g_object_unref(second);g_assert_true(gimp_painter_binding_close(G_OBJECT(gimp),nullptr));
}
int main(int argc,char**argv)
{
  g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp=gimp_init_for_testing();
  g_test_add_func("/painter-options/generated-properties",generated_properties);
  g_test_add_func("/painter-options/all-native-properties-config",all_native_properties_config);
  g_test_add_func("/painter-options/legacy-brush-mode-config",legacy_brush_mode_config);
  g_test_add_func("/painter-options/edit-curve-commit-conflict",edit_curve_commit_and_conflict);
  g_test_add_func("/painter-options/history-config",history_and_config_roundtrip);
  g_test_add_func("/painter-options/draft-name-identity-commit",draft_name_identity_and_commit);
  g_test_add_func("/painter-options/draft-name-history-copy",draft_name_history_and_copy);
  g_test_add_func("/painter-options/draft-name-external-errors",draft_name_external_settings_and_errors);
  g_test_add_func("/painter-options/draft-name-notification-lifetime",draft_name_notification_lifetime);
  g_test_add_func("/painter-options/draft-name-reentry",draft_name_reentry);
  g_test_add_func("/painter-options/native-config-reset-defaults",native_config_reset_defaults);
  g_test_add_func("/painter-options/notification-last-ref",notification_releases_caller);
  g_test_add_func("/painter-options/closed-invalid-curve",closed_and_invalid_curves);
  g_test_add_func("/painter-options/named-resources",named_resources);
  g_test_add_func("/painter-options/selection-reentry",selection_reentry);
  g_test_add_func("/painter-options/commit-close",commit_closes_options);
  g_test_add_func("/painter-options/cross-options-history",shared_application_history);
  g_test_add_func("/painter-options/application-history-close",shared_history_application_close);
  int result=g_test_run();gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_BUILDDIR","app/tests/gimpdir-output");gimp_exit(gimp,TRUE);return result;
}
