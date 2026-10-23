#include "config.h"
#include "painter/binding-store.hpp"
#include "painter/boundary.hpp"
#include "painter/gimp-painter-error.h"
#include <stdexcept>
#include <new>
static int armed=0, consumed=0;
extern "C" void arm_fault(int kind){armed=kind;consumed=0;}
extern "C" int consumed_fault(){return consumed;}
extern "C" GimpPainter::BindingStore& __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*);
extern "C" GimpPainter::BindingStore& __wrap__ZN11GimpPainter12BindingStore6ensureEP8_GObject(GObject*o){
 if(armed && g_strcmp0(G_OBJECT_TYPE_NAME(o),"GimpPainterLayerTiles")==0){int k=armed;armed=0;++consumed;
  switch(k){case 1:throw GimpPainter::Error(GIMP_PAINTER_ERROR_INVALID_STATE,"initializer injected typed fault");case 2:throw std::runtime_error("initializer injected standard fault");case 3:throw std::bad_alloc();default:throw 17;}}
 return __real__ZN11GimpPainter12BindingStore6ensureEP8_GObject(o);
}
