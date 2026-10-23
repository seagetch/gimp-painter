/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Native ABI, independent-worker observation and generation-safe owner events. */
#include "core/gimpprogress.h"
#include "core/gimpfilterprocedure-progress.h"

static void
progress_native_abi (void)
{
  GimpImage *image = image_new (8,8);
  GimpFilterLayer *layer = filter_new (image,NULL,8,8);
  GimpProgress *progress = GIMP_PROGRESS (layer);
  GimpFilterLayerProgress state;
  guint64 generation = gimp_filter_layer_get_generation (layer);
  gchar text[2048]; memset (text,'x',sizeof text); text[sizeof text-1] = 0;
  g_assert_true (GIMP_IS_PROGRESS (layer));
  g_assert_false (gimp_filter_procedure_progress_is_private (NULL));
  g_assert_false (gimp_filter_procedure_progress_is_private (progress));
  g_assert_true (gimp_progress_start (progress,FALSE,"%s","Exact start message") == progress);
  g_assert_true (gimp_filter_layer_get_progress (layer,&state));
  g_assert_true (state.active); g_assert_false (state.cancellable);
  g_assert_cmpstr (state.text,==,"Exact start message");
  g_assert_null (gimp_progress_start (progress,TRUE,"busy"));
  gimp_progress_cancel (progress);
  g_assert_cmpuint (generation,==,gimp_filter_layer_get_generation (layer));
  gimp_progress_set_text_literal (progress,text);
  g_assert_true (gimp_filter_layer_get_progress (layer,&state));
  g_assert_cmpuint (strlen(state.text),==,512);
  g_assert_true (g_utf8_validate (state.text,-1,NULL));
  gimp_progress_set_value (progress,0.375);
  g_assert_cmpfloat (gimp_progress_get_value(progress),==,0.375);
  GIMP_PROGRESS_GET_IFACE(progress)->set_value(progress,NAN);
  g_assert_cmpfloat (gimp_progress_get_value(progress),==,0.375);
  gimp_progress_pulse(progress);
  g_assert_true(gimp_progress_message(progress,gimp,GIMP_MESSAGE_WARNING,"native-test","Bounded warning"));
  gimp_filter_layer_get_progress(layer,&state);
  g_assert_cmpuint(state.pulses,==,1); g_assert_cmpuint(state.message_revision,==,1);
  g_assert_cmpstr(state.message_text,==,"Bounded warning");
  gimp_progress_end(progress); g_assert_false(gimp_progress_is_active(progress));
  gimp_progress_set_value(progress,0.9); g_assert_cmpfloat(gimp_progress_get_value(progress),==,0.0);
  g_assert_true(gimp_progress_start(progress,TRUE,"cancel me")==progress);
  gimp_progress_cancel(progress);
  g_assert_cmpint(gimp_filter_layer_get_state(layer),==,GIMP_FILTER_LAYER_FAILED);
  g_assert_false(gimp_progress_is_active(progress));
  g_assert_false(gimp_filter_layer_cancel_generation(layer,generation));
  g_object_unref(image);
}

typedef struct { guint action, count; gboolean acted; GimpImage *image; } ProgressReentry;
static void
progress_reenter (GimpFilterLayer *layer, gpointer data)
{
  ProgressReentry *state = data;
  ++state->count;
  if (state->acted || !gimp_progress_is_active(GIMP_PROGRESS(layer))) return;
  state->acted = TRUE;
  if (state->action == 0) gimp_progress_cancel(GIMP_PROGRESS(layer));
  else if (state->action == 1) define_edge(layer);
  else if (state->action == 2) gimp_painter_binding_close(G_OBJECT(layer),NULL);
  else { g_object_unref(state->image); state->image=NULL; }
}
static void
progress_start_reentry (void)
{
  for (guint action=0; action<4; ++action) {
    GimpImage *image=image_new(8,8);
    GimpFilterLayer *layer=filter_new(image,NULL,8,8);
    ProgressReentry state={action,0,FALSE,image};
    g_object_ref(layer);
    g_signal_connect(layer,"filter-progress-changed",G_CALLBACK(progress_reenter),&state);
    g_assert_null(gimp_progress_start(GIMP_PROGRESS(layer),TRUE,"reentry"));
    g_assert_true(state.acted);
    g_assert_false(gimp_progress_is_active(GIMP_PROGRESS(layer)));
    if(action>=2) {
      const guint count=state.count; spin_ms(20);
      g_assert_cmpuint(state.count,==,count);
      g_assert_cmpint(gimp_filter_layer_get_state(layer),==,GIMP_FILTER_LAYER_CLOSED);
    }
    if(state.image) g_object_unref(state.image);
    g_object_unref(layer);
  }
}

static void
progress_define_route (GimpFilterLayer *layer, guint route)
{
  if (route == 0) define_edge(layer);
  else if (route == 1) define_blinds(layer,63,7,1,0);
  else if (route == 2) small_tiles_define(layer,3);
  else if (route == 3) retinex_define(layer,100,2,0,1.2);
  else {
    GimpValueArray *args=convolution_args(convolution_identity,convolution_all,1.0,0.0,0);
    convolution_define(layer,args); gimp_value_array_unref(args);
  }
}
typedef struct {
  guint changes, intermediate, pulses;
  gdouble previous;
  gboolean ended;
  GThread *owner;
  const gchar *worker_text;
  gboolean saw_worker_text;
} ProgressObserved;
static void
progress_observe (GimpFilterLayer *layer, gpointer data)
{
  ProgressObserved *observed=data;
  GimpFilterLayerProgress state;
  g_assert_true(g_thread_self()==observed->owner);
  g_assert_true(gimp_filter_layer_get_progress(layer,&state));
  ++observed->changes;
  if(observed->worker_text && strstr(state.text,observed->worker_text)) observed->saw_worker_text=TRUE;
  if(state.active) {
    g_assert_cmpuint(state.generation,==,gimp_filter_layer_get_generation(layer));
    g_assert_true(state.cancellable);
    g_assert_true(isfinite(state.value));
    g_assert_cmpfloat(state.value,>=,observed->previous);
    g_assert_cmpfloat(state.value,<=,1.0);
    observed->previous=state.value;
    if(gimp_filter_layer_get_state(layer)==GIMP_FILTER_LAYER_RUNNING && state.value>0.15 && state.value<0.85)
      ++observed->intermediate;
  } else observed->ended=TRUE;
}
static gboolean progress_heartbeat (gpointer data) { ++*(guint *)data; return G_SOURCE_CONTINUE; }
static void
progress_real_workers (void)
{
  for(guint route=0;route<5;++route) {
    const gint size=route==0?2048:1024;
    GimpImage *image=image_new(size,size);
    GimpLayer *source=source_new(image,NULL,size,size);
    GimpFilterLayer *layer=filter_new(image,NULL,size,size);
    const gchar *texts[]={NULL,"Adding blinds","Tiling","Retinex","Convolution Matrix"};
    ProgressObserved observed={0,0,0,0,FALSE,g_thread_self(),texts[route],FALSE};
    guint heartbeats=0, heartbeat=g_timeout_add(2,progress_heartbeat,&heartbeats);
    fill(source,43,97,181,255);
    progress_define_route(layer,route);
    g_signal_connect(layer,"filter-progress-changed",G_CALLBACK(progress_observe),&observed);
    const gint64 started=g_get_monotonic_time();
    settle(layer);
    const gint64 elapsed=g_get_monotonic_time()-started;
    g_source_remove(heartbeat);
    g_assert_false(gimp_progress_is_active(GIMP_PROGRESS(layer)));
    g_assert_true(observed.ended);
    g_assert_cmpuint(observed.changes,>,2);
    g_assert_cmpuint(heartbeats,>,5);
    g_test_message("route=%u progress-signals=%u worker-intermediate=%u heartbeats=%u elapsed-us=%"G_GINT64_FORMAT" owner-max-us=%"G_GINT64_FORMAT,
      route,observed.changes,observed.intermediate,heartbeats,elapsed,gimp_filter_layer_get_max_quantum_us(layer));
    /* Short native kernels can finish between two paced owner samples. Their
     * real internal checkpoints are separately proven by the kernel target. */
    if(route==3) g_assert_cmpuint(observed.intermediate,>,0);
    if(route) g_assert_true(observed.saw_worker_text);
    g_object_unref(image);
  }
}
static void
progress_worker_cancel_replace_close (void)
{
  for(guint route=0;route<5;++route)
    for(guint action=0;action<3;++action) {
      GimpImage *image=image_new(512,512);
      GimpLayer *source=source_new(image,NULL,512,512);
      GimpFilterLayer *layer=filter_new(image,NULL,512,512);
      GimpFilterLayerProgress state;
      fill(source,43,97,181,255); progress_define_route(layer,route);
      fill(GIMP_LAYER(layer),17,29,53,123);gimp_filter_layer_mark_as_loaded(layer);
      gimp_filter_layer_invalidate(layer);
      blinds_wait(layer,GIMP_FILTER_LAYER_RUNNING);
      g_assert_true(gimp_filter_layer_get_progress(layer,&state));
      g_assert_true(state.active);g_assert_true(state.cancellable);
      const guint64 obsolete=state.generation;
      if(action==0) {
        gimp_progress_cancel(GIMP_PROGRESS(layer));
        g_assert_cmpint(gimp_filter_layer_get_state(layer),==,GIMP_FILTER_LAYER_FAILED);
        spin_ms(40);pixel(GIMP_LAYER(layer),17,29,53,123);
      } else if(action==1) {
        define_edge(layer);
        g_assert_false(gimp_filter_layer_cancel_generation(layer,obsolete));
        settle(layer);pixel(GIMP_LAYER(layer),0,0,0,255);
      } else {
        g_object_ref(layer);g_object_unref(image);image=NULL;
        spin_ms(40);g_assert_false(gimp_progress_is_active(GIMP_PROGRESS(layer)));
        g_assert_cmpint(gimp_filter_layer_get_state(layer),==,GIMP_FILTER_LAYER_CLOSED);
        g_object_unref(layer);
      }
      if(image)g_object_unref(image);
    }
}

static void
progress_independent_owners (void)
{
  GimpImage *a=image_new(512,512),*b=image_new(512,512);
  GimpLayer *sa=source_new(a,NULL,512,512),*sb=source_new(b,NULL,512,512);
  GimpFilterLayer *fa=filter_new(a,NULL,512,512),*fb=filter_new(b,NULL,512,512);
  fill(sa,41,83,127,255);fill(sb,71,91,117,255);
  retinex_define(fa,100,2,0,1.2);retinex_define(fb,100,2,0,1.2);
  blinds_wait(fa,GIMP_FILTER_LAYER_RUNNING);blinds_wait(fb,GIMP_FILTER_LAYER_RUNNING);
  g_assert_true(gimp_progress_is_active(GIMP_PROGRESS(fa)));
  g_assert_true(gimp_progress_is_active(GIMP_PROGRESS(fb)));
  const guint64 token=gimp_filter_layer_get_generation(fb);
  gimp_progress_cancel(GIMP_PROGRESS(fa));
  g_assert_false(gimp_progress_is_active(GIMP_PROGRESS(fa)));
  g_assert_true(gimp_progress_is_active(GIMP_PROGRESS(fb)));
  g_assert_cmpuint(token,==,gimp_filter_layer_get_generation(fb));
  settle(fb);g_assert_cmpint(gimp_filter_layer_get_state(fa),==,GIMP_FILTER_LAYER_FAILED);
  g_object_unref(a);g_object_unref(b);
}
