#define main original_tool_main
#include "test-painter-mypaint-tool.cpp"
#undef main
#include <execinfo.h>
#include <dlfcn.h>
static bool during_setting=false;
static void(*prior_control)(GimpTool*,GimpToolAction,GimpDisplay*)=nullptr;
static void traced_control(GimpTool*t,GimpToolAction a,GimpDisplay*d){if(during_setting){g_print("SETTING_CONTROL action=%d\n",a);void*frames[50];int n=backtrace(frames,50);for(int i=0;i<n;++i){Dl_info info{};if(dladdr(frames[i],&info))g_print("FRAME %s %p\n",info.dli_fname,(void*)((char*)frames[i]-(char*)info.dli_fbase));}}prior_control(t,a,d);}
extern "C" void __real_gimp_tool_control_halt(GimpToolControl*);
extern "C" void __wrap_gimp_tool_control_halt(GimpToolControl*c){if(during_setting){void*frames[50];int n=backtrace(frames,50);for(int i=0;i<n;++i){Dl_info info{};if(dladdr(frames[i],&info))g_print("HALTFRAME %s %p\n",info.dli_fname,(void*)((char*)frames[i]-(char*)info.dli_fbase));}}__real_gimp_tool_control_halt(c);}
static void trace(){Scene s;s.motion(false);s.press();s.coords.x+=20;s.motion(true,75);auto*klass=GIMP_TOOL_GET_CLASS(s.tool);prior_control=klass->control;klass->control=traced_control;during_setting=true;g_object_set(s.options,"radius-logarithmic",2.,nullptr);during_setting=false;klass->control=prior_control;g_print("AFTER_SETTING active=%d depth=%d\n",gimp_tool_control_is_active(s.tool->control),s.depth());}
int main(int argc,char**argv){g_test_init(&argc,&argv,nullptr);gimp_test_utils_set_gimp3_directory("GIMP_TESTING_ABS_TOP_SRCDIR","app/tests/gimpdir");gimp_test_utils_setup_menus_path();gimp=gimp_init_for_gui_testing(TRUE);g_object_set(gimp->config,"painter-canvas-ui",TRUE,nullptr);g_test_add_func("/physical-gesture/trace",trace);g_application_run(gimp->app,0,nullptr);int result=gimp_core_app_get_exit_status(GIMP_CORE_APP(gimp->app));g_application_quit(G_APPLICATION(gimp->app));g_clear_object(&gimp->app);return result;}
