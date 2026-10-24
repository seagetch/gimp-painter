/* External observation only: enumerate top-level X11 windows, resize the main
 * GIMP window for portrait evidence, and capture the Xvfb root framebuffer. */
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
 gtk_init(&argc,&argv);
 Display *d=GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
 Window root=DefaultRootWindow(d),r,p,*children=NULL;unsigned int count=0;
 if(XQueryTree(d,root,&r,&p,&children,&count)) {
  for(unsigned int i=0;i<count;i++) {
   char *name=NULL; XWindowAttributes a;
   XFetchName(d,children[i],&name); XGetWindowAttributes(d,children[i],&a);
   if(!name || !*name) {
    Atom type;int format;unsigned long n,remaining;unsigned char *value=NULL;
    if(XGetWindowProperty(d,children[i],XInternAtom(d,"_NET_WM_NAME",False),0,4096,False,AnyPropertyType,&type,&format,&n,&remaining,&value)==Success && value) {
     if(name)XFree(name);name=(char*)value;
    }
   }
   if(a.map_state==IsViewable) {
    printf("WINDOW %lu %dx%d name=%s\n",children[i],a.width,a.height,name?name:"");
    if(name && strstr(name,"Welcome to GIMP")) {
     XEvent ev={0};ev.xclient.type=ClientMessage;ev.xclient.window=children[i];
     ev.xclient.message_type=XInternAtom(d,"WM_PROTOCOLS",False);ev.xclient.format=32;
     ev.xclient.data.l[0]=XInternAtom(d,"WM_DELETE_WINDOW",False);ev.xclient.data.l[1]=CurrentTime;
     XSendEvent(d,children[i],False,NoEventMask,&ev);
     printf("ACTION close welcome via WM_DELETE_WINDOW\n");
    } else {
     XClassHint hint={0};gboolean is_gimp=FALSE;
     if(XGetClassHint(d,children[i],&hint)) {
      is_gimp=(hint.res_class && (strstr(hint.res_class,"Gimp") || strstr(hint.res_class,"gimp")));
      if(hint.res_name)XFree(hint.res_name);if(hint.res_class)XFree(hint.res_class);
     }
     if((name && strstr(name,"GNU Image Manipulation Program")) || (is_gimp && a.width>=700)) {
      XMoveResizeWindow(d,children[i],0,0,1040,1800);
      printf("ACTION resize main window 1040x1800\n");
     }
    }
   }
   if(name)XFree(name);
  }
  if(children)XFree(children);
 }
 XSync(d,False);g_usleep(1500000);
 if(argc>1) {
  GdkWindow *w=gdk_get_default_root_window();
  GdkPixbuf *pixels=gdk_pixbuf_get_from_window(w,0,0,gdk_window_get_width(w),gdk_window_get_height(w));
  GError *error=NULL;
  if(!pixels || !gdk_pixbuf_save(pixels,argv[1],"png",&error,NULL)) {
   fprintf(stderr,"capture failed: %s\n",error?error->message:"no pixels");return 1;
  }
  g_object_unref(pixels);
 }
 return 0;
}
