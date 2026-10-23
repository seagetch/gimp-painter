#include "config.h"
#include "painter/boundary.hpp"
#include <glib-object.h>
#include <execinfo.h>
#include <new>
#include <stdexcept>
static GObject *target=nullptr;
static int kind=0,count=0,unwound=0;
extern "C" gpointer __real_g_weak_ref_get(GWeakRef*);
extern "C" gpointer __wrap_g_weak_ref_get(GWeakRef*);
extern "C" void dispatch_arm(GObject*o,int k){target=o;kind=k;count=unwound=0;}
extern "C" int dispatch_consumed(){return !kind && count==1 && unwound==1;}
extern "C" int dispatch_unconsumed(){return kind && count==0 && unwound==0;}
extern "C" void dispatch_disarm(){target=nullptr;kind=0;}
extern "C" gpointer __wrap_g_weak_ref_get(GWeakRef*ref){
 gpointer result=__real_g_weak_ref_get(ref);
 if(kind && result==target){
  // The real GLib function has RETURNED. Drop its temporary lease before the
  // test-only C++ throw, directly back into C++ WeakRef::lock/dispatch.
  int k=kind;kind=0;++count;g_object_unref(result);
  void*frames[12];int n=backtrace(frames,12);backtrace_symbols_fd(frames,n,2);
  struct Guard{~Guard()noexcept{++unwound;}} guard;
  if(k==1)throw GimpPainter::Error(GIMP_PAINTER_ERROR_INVALID_STATE,"dispatch typed fault");
  if(k==2)throw std::runtime_error("dispatch standard fault");
  if(k==3)throw std::bad_alloc();
  throw 17;
 }
 return result;
}
