/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gtk/gtk.h>
#include <gegl.h>
extern "C" {
#include "libgimpbase/gimpbase.h"
#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpprogress.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
#include "widgets/widgets-types.h"
#include "widgets/gimpfiledialog.h"
#include "plug-in/gimppluginprocedure.h"
#include "file-save-deferred.h"
}
#include "painter/binding-store.hpp"
#include "painter/gimp-painter-binding.h"
#include "painter/connection.hpp"
#include "painter/source.hpp"
#include "gimp-intl.h"
#include <vector>
using namespace GimpPainter;
struct GimpDeferredSave { GObject parent; };
struct GimpDeferredSaveClass { GObjectClass parent; };
static GType gimp_deferred_save_get_type (void);
G_DEFINE_TYPE (GimpDeferredSave, gimp_deferred_save, G_TYPE_OBJECT)
namespace GimpPainter {
template<> struct TypeTraits<GimpDeferredSave> { static GType type(){return gimp_deferred_save_get_type();} };
}
namespace {
struct Request {
 ObjectRef<GObject> gimp,image,file,procedure;
 WeakRef<GObject> owner,progress;
 GimpRunMode run_mode;
 bool change_saved_state,export_backward,export_forward,compression,verbose;
 FileSaveCompletion completion=nullptr;gpointer data=nullptr;GDestroyNotify destroy=nullptr;
 ~Request(){if(destroy)destroy(data);}
};
struct Pending {
 std::shared_ptr<Request> request;
 Source source;std::vector<Connection> connections;bool closed=false,running=false;
 void close()noexcept {closed=true;source.close();auto old_connections=std::move(connections);auto old_request=std::move(request);}
};
struct SaveSlot:SlotSpec<GimpDeferredSave,Pending>{};
void close_save(GObject*self){gimp_painter_binding_close(self,nullptr);}
void destroyed(GtkWidget*,gpointer self){close_save(G_OBJECT(self));}
void display_changed(GObject*display,GParamSpec*,gpointer self){
 boundary_void(nullptr,[&]{auto*store=BindingStore::find(G_OBJECT(self));if(!store||store->state()!=BindingStore::State::active)return;
  bool changed=store->read<SaveSlot>([&](const Pending&p){return p.request&&gimp_display_get_image(GIMP_DISPLAY(display))!=GIMP_IMAGE(p.request->image.get());});
  if(changed)close_save(G_OBJECT(self));});
}
void response(GtkDialog*dialog,gint id,gpointer self){
 if(id!=GTK_RESPONSE_OK&&id!=GTK_RESPONSE_HELP){close_save(G_OBJECT(self));gtk_widget_destroy(GTK_WIDGET(dialog));}
}
void connect_owner(GimpDeferredSave*self,Pending&p,const ObjectRef<GObject>&owner){
 // Source owns the operation until completion; connections borrow that native
 // owner only while registered, and close disconnects them before release.
 if(GTK_IS_WIDGET(owner.get()))p.connections.push_back(Connection::connect(owner,"destroy",G_CALLBACK(destroyed),self,nullptr));
 if(GIMP_IS_FILE_DIALOG(owner.get()))p.connections.push_back(Connection::connect(owner,"response",G_CALLBACK(response),self,nullptr));
 if(GIMP_IS_DISPLAY(owner.get())){
  p.connections.push_back(Connection::connect(owner,"notify::image",G_CALLBACK(display_changed),self,nullptr));
  auto*shell=gimp_display_get_shell(GIMP_DISPLAY(owner.get()));
  if(shell)p.connections.push_back(Connection::connect(ObjectRef<GObject>::retain(G_OBJECT(shell)),"destroy",G_CALLBACK(destroyed),self,nullptr));
 }
}
bool attempt(GimpDeferredSave*self){
 auto&store=BindingStore::require(G_OBJECT(self));
 return store.with<SaveSlot>([&](Pending&p){
  if(p.closed||p.running||!p.request)return false;
  auto request=p.request;auto owner=request->owner.lock();auto progress=request->progress.lock();
  if(!owner||(GIMP_IS_FILE_DIALOG(owner.get())&&!GIMP_FILE_DIALOG(owner.get())->progress)||(GIMP_IS_DISPLAY(owner.get())&&gimp_display_get_image(GIMP_DISPLAY(owner.get()))!=GIMP_IMAGE(request->image.get()))){p.close();return false;}
  auto*image=GIMP_IMAGE(request->image.get());
  const bool pending=gimp_image_has_pending_paint(image);
  if(p.closed)return false;
  if(pending)return true;
  struct Running{bool&v;Running(bool&b):v(b){v=true;}~Running(){v=false;}} running(p.running);
  gboolean busy=FALSE;
  gboolean success=file_save_dialog_save_image_pending(progress?GIMP_PROGRESS(progress.get()):nullptr,GIMP(request->gimp.get()),image,G_FILE(request->file.get()),GIMP_PLUG_IN_PROCEDURE(request->procedure.get()),request->run_mode,request->change_saved_state,request->export_backward,request->export_forward,request->compression,request->verbose,&busy);
  if(p.closed)return false;
  if(busy){gimp_image_saving(image);return !p.closed;}
  p.close();
  if(request->completion)request->completion(success,request->data);
  return false;
 });
}
void constructed(GObject*self){G_OBJECT_CLASS(gimp_deferred_save_parent_class)->constructed(self);boundary_void(nullptr,[&]{BindingStore::require(self).activate();});}
void dispose(GObject*self){close_save(self);G_OBJECT_CLASS(gimp_deferred_save_parent_class)->dispose(self);}
}
static void gimp_deferred_save_class_init(GimpDeferredSaveClass*k){G_OBJECT_CLASS(k)->constructed=constructed;G_OBJECT_CLASS(k)->dispose=dispose;}
static void gimp_deferred_save_init(GimpDeferredSave*self){boundary_void(nullptr,[&]{BindingStore::ensure(G_OBJECT(self)).emplace<SaveSlot>();});}
void file_save_dialog_save_image_async(GObject*owner,GimpProgress*progress,Gimp*gimp,GimpImage*image,GFile*file,GimpPlugInProcedure*procedure,GimpRunMode mode,gboolean change,gboolean backward,gboolean forward,gboolean compression,gboolean verbose,FileSaveCompletion completion,gpointer data,GDestroyNotify destroy){
 // Own completion data even if validation/allocation fails before scheduling.
 bool transferred=false;
 boundary_void(nullptr,[&]{
  auto request=std::make_shared<Request>();request->completion=completion;request->data=data;request->destroy=destroy;transferred=true;
  request->gimp=ObjectRef<GObject>::retain(G_OBJECT(gimp));request->image=ObjectRef<GObject>::retain(G_OBJECT(image));request->file=ObjectRef<GObject>::retain(G_OBJECT(file));request->procedure=ObjectRef<GObject>::retain(G_OBJECT(procedure));
  request->owner=WeakRef<GObject>(ObjectRef<GObject>::retain(owner));request->progress=WeakRef<GObject>(ObjectRef<GObject>::retain(progress?G_OBJECT(progress):nullptr));
  request->run_mode=mode;request->change_saved_state=change;request->export_backward=backward;request->export_forward=forward;request->compression=compression;request->verbose=verbose;
  auto self=ObjectRef<GimpDeferredSave>::adopt(static_cast<GimpDeferredSave*>(g_object_new(gimp_deferred_save_get_type(),nullptr)));
  BindingStore::require(G_OBJECT(self.get())).with<SaveSlot>([&](Pending&p){p.request=request;connect_owner(self.get(),p,ObjectRef<GObject>::retain(owner));});
  if(GIMP_IS_FILE_DIALOG(owner)&&!GIMP_FILE_DIALOG(owner)->progress)return;
  if(gimp_image_has_pending_paint(image))gimp_image_saving(image);
  auto*store=BindingStore::find(G_OBJECT(self.get()));if(!store||store->state()!=BindingStore::State::active)return;
  if(!attempt(self.get()))return;
  store->with<SaveSlot>([&](Pending&p){
   if(p.closed)return;
   // A completion-owned reference, released when close destroys this source.
   // No nested main loop, paint drain, retry count or input cancellation.
   p.source=Source::timeout(nullptr,25,G_PRIORITY_DEFAULT_IDLE,[self]{return attempt(self.get());});
  });
  auto live_progress=request->progress.lock();
  if(live_progress)gimp_message_literal(gimp,live_progress.get(),GIMP_MESSAGE_INFO,_("Waiting for painting to finish before saving."));
 });
 if(!transferred&&destroy)destroy(data);
}
