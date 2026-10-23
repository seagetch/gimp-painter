from pathlib import Path
import subprocess
b=Path(__file__).resolve().parent; g=b/'gimp'
def orig(p): return subprocess.check_output(['git','-C',str(g),'show','HEAD:'+p],text=True)
header='''#ifndef LEGACY_TIMELINE_H
#define LEGACY_TIMELINE_H
/* Observation only: ordinal counts run attempts; legacy has no generation token. */
static inline void legacy_timeline(GObject *o, const char *event) {
 if (!GIMP_IS_ITEM(o)) return;
 g_print("TL time_us=%" G_GINT64_FORMAT " event=%s ptr=%p id=%d ordinal=%d attached=%d name=%s\\n",g_get_monotonic_time(),event,o,gimp_item_get_ID(GIMP_ITEM(o)),GPOINTER_TO_INT(g_object_get_data(o,"timeline-ordinal")),gimp_item_is_attached(GIMP_ITEM(o)),gimp_object_get_name(o));
}
#endif
'''
(g/'app/core/legacy-timeline.h').write_text(header)
s=orig('app/pdb/pdb-cxx-utils.hpp'); s=s.replace('#include "base/scopeguard.hpp"','#include "core/legacy-timeline.h"\n#include "base/scopeguard.hpp"')
s=s.replace('  bool run(ContextArgs... cargs) {','  bool run(ContextArgs... cargs) {\n    g_object_set_data(G_OBJECT(progress),"timeline-ordinal",GINT_TO_POINTER(GPOINTER_TO_INT(g_object_get_data(G_OBJECT(progress),"timeline-ordinal"))+1));\n    legacy_timeline(G_OBJECT(progress),"run-enter");')
s=s.replace('      running = true;','      running = true;\n      legacy_timeline(G_OBJECT(progress),"run-return-running");')
s=s.replace('  void stop() {','  void stop() {\n    legacy_timeline(G_OBJECT(progress),running ? "stop-request-running" : "stop-request-idle");')
s=s.replace('      gimp_progress_cancel(progress);','      { legacy_timeline(G_OBJECT(progress),"cancel-signal"); gimp_progress_cancel(progress); }')
s=s.replace('  void on_end() {','  void on_end() {\n    legacy_timeline(G_OBJECT(progress),"runner-on-end");')
(g/'app/pdb/pdb-cxx-utils.hpp').write_text(s)
s=orig('app/core/gimpfilterlayer.cpp')
s=s.replace('      copy_region_nocow(&srcPR, &destPR);','      legacy_timeline(g_object,"cache-input-copy-begin");\n      copy_region_nocow(&srcPR, &destPR);\n      legacy_timeline(g_object,"cache-input-copy-end");')
s=s.replace('  GIMP_DRAWABLE_CLASS(Class::parent_class)->project_region (self, x, y, width, height, projPR, combine);','  legacy_timeline(g_object,"composite-cache");\n  GIMP_DRAWABLE_CLASS(Class::parent_class)->project_region (self, x, y, width, height, projPR, combine);')
s=s.replace('void GLib::FilterLayer::notify_filter_end()\n{','void GLib::FilterLayer::notify_filter_end()\n{\n  legacy_timeline(g_object,"propagation-begin");')
s=s.replace('\n\nvoid GLib::FilterLayer::update ()', '\n\nvoid GLib::FilterLayer::update ()')
a=s.index('void GLib::FilterLayer::notify_filter_end()'); z=s.index('\n\nvoid GLib::FilterLayer::update ()',a); part=s[a:z]; pos=part.rfind('}'); part=part[:pos]+'  legacy_timeline(g_object,"propagation-end");\n'+part[pos:];s=s[:a]+part+s[z:]
s=s.replace('  filter_progress = percentage;', '  legacy_timeline(g_object,"progress-value");\n  g_object_set_data(g_object,"timeline-progress-seen",GINT_TO_POINTER(1));\n  filter_progress = percentage;')
s=s.replace('void GLib::FilterLayer::end()\n{','void GLib::FilterLayer::end()\n{\n  legacy_timeline(g_object,"progress-end-callback");')
s=s.replace('    runner      = std::move(new_runner);','    legacy_timeline(g_object,"new-runner-requeue");\n    runner      = std::move(new_runner);')
s=s.replace('    filter_reset();\n    invalidate_layer();\n\n  } else if (!waiting_process_stack)', '    legacy_timeline(g_object,"stack-unblocked-requeue");\n    filter_reset();\n    invalidate_layer();\n\n  } else if (!waiting_process_stack)')
s+='''\nextern "C" gint gimp_filter_layer_capture_pending(GimpFilterLayer* layer) {
 auto* impl=dynamic_cast<GLib::FilterLayer*>(FilterLayerInterface::cast(layer));
 return (impl->is_waiting_to_be_processed()?1:0)|(impl->waiting_for_runner?2:0);
}\n'''
(g/'app/core/gimpfilterlayer.cpp').write_text(s)
s=orig('app/core/gimpdrawable-shadow.c');s=s.replace('#include "gimpdrawable.h"','#include "gimpdrawable.h"\n#include "legacy-timeline.h"');a=s.index('gimp_drawable_merge_shadow_tiles (');a=s.index('{',a);s=s[:a+1]+'\n  legacy_timeline(G_OBJECT(drawable),"shadow-merge-enter");'+s[a+1:];s=s.replace('      tile_manager_unref (tiles);','      legacy_timeline(G_OBJECT(drawable),"shadow-merge-applied");\n      tile_manager_unref (tiles);');(g/'app/core/gimpdrawable-shadow.c').write_text(s)
