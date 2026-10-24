/* Defined bounded queue host; actual wrapper/compressor bodies are unchanged. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int gboolean;
#define TRUE 1
#define FALSE 0
#define GDK_MOTION_NOTIFY 3
#define GDK_BUTTON_RELEASE 4
#define GDK_KEY_PRESS 5
#define GDK_CROSSING 6
typedef struct {int id;} GtkWidget;
typedef struct {GtkWidget *widget;} Window;
typedef struct {struct {int type;Window *window;} any;struct {unsigned state;void *device;} motion;unsigned id;} GdkEvent;
typedef struct {int ignored;} GimpDisplayShell;
static GtkWidget widgets[2]={{0},{1}};
static Window windows[3]={{&widgets[0]},{&widgets[0]},{&widgets[1]}};
static int devices[2];
static GdkEvent *queue[160];
static unsigned count,position,freed[160],dispatch[160],dispatch_count,acquired[160],scenes;
static int gtk_widget_get_realized(GtkWidget *w){return w!=NULL;}
#define g_return_val_if_fail(condition,value) do{if(!(condition))return value;}while(0)
static GtkWidget *gtk_get_event_widget(GdkEvent *e){return e->any.window->widget;}
static int gdk_events_pending(void){return position<count;}
static GdkEvent *gdk_event_get(void){GdkEvent*e=queue[position++];if(e)acquired[e->id]++;return e;}
static void gdk_event_free(GdkEvent*e){assert(e&&e->id<160);assert(freed[e->id]++==0);}
static void gtk_main_do_event(GdkEvent*e){assert(!freed[e->id]);dispatch[dispatch_count++]=e->id;}
static GdkEvent *gimp_display_shell_compress_motion(GdkEvent*,GdkEvent**);
static gboolean gimp_display_shell_canvas_tool_events_internal(GtkWidget*w,GdkEvent*e,GimpDisplayShell*s,GdkEvent**next){
 (void)w;(void)s;GdkEvent*last=gimp_display_shell_compress_motion(e,next);gtk_main_do_event(last?last:e);if(last)gdk_event_free(last);return TRUE;
}
#include "old-wrapper.inc"
#include "old-compressor.inc"
static GdkEvent event(unsigned id){GdkEvent e={{GDK_MOTION_NOTIFY,&windows[0]},{17,&devices[0]},id};return e;}
static void one(unsigned prefix,unsigned boundary,unsigned tail,int null_slot){
 GdkEvent events[160];GimpDisplayShell shell={0};unsigned next_id=1,stop_id=0;
 memset(freed,0,sizeof freed);memset(acquired,0,sizeof acquired);memset(dispatch,0,sizeof dispatch);position=count=dispatch_count=0;events[0]=event(0);
 for(unsigned i=0;i<prefix;i++){events[next_id]=event(next_id);queue[count++]=&events[next_id++];}
 if(null_slot)queue[count++]=NULL;
 if(boundary){stop_id=next_id;events[next_id]=event(next_id);
  switch(boundary){case 1:events[next_id].any.type=GDK_BUTTON_RELEASE;break;case 2:events[next_id].any.type=GDK_KEY_PRESS;break;case 3:events[next_id].any.type=GDK_CROSSING;break;case 4:events[next_id].motion.state^=1;break;case 5:events[next_id].motion.device=&devices[1];break;case 6:events[next_id].any.window=&windows[1];break;case 7:events[next_id].any.window=&windows[2];break;default:assert(0);}
  queue[count++]=&events[next_id++];}
 for(unsigned i=0;i<tail;i++){events[next_id]=event(next_id);queue[count++]=&events[next_id++];}
 assert(gimp_display_shell_canvas_tool_events(&widgets[0],&events[0],&shell));
 /* Independent expected boundary indices: no algorithm emulates queue scanning. */
 unsigned consumed=boundary?prefix:prefix+tail;assert(dispatch_count==(boundary?2u:1u));assert(dispatch[0]==consumed);if(boundary)assert(dispatch[1]==stop_id);
 assert(position==(boundary?prefix+1+(unsigned)null_slot:count));assert(freed[0]==0&&acquired[0]==0);
 for(unsigned i=1;i<next_id;i++){unsigned expected=boundary?i<=stop_id:1;assert(freed[i]==expected);assert(acquired[i]==expected);}
 scenes++;
}
int main(void){
 for(unsigned n=0;n<=64;n++)for(unsigned boundary=0;boundary<=7;boundary++)for(unsigned tail=0;tail<4;tail++)for(int null_slot=0;null_slot<2;null_slot++)one(n,boundary,tail,null_slot);
 for(unsigned type=4;type<=6;type++){
  GdkEvent initial=event(0),queued=event(1),*next=(GdkEvent*)1;initial.any.type=type;queue[0]=&queued;count=1;position=0;assert(!gimp_display_shell_compress_motion(&initial,&next));assert(!next&&position==0);scenes++;
 }
 printf("PASS %u bounded old wrapper/compressor scenes; borrowed initial untouched, acquired prefix/terminal freed exactly once, tail unchanged\n",scenes);
}
