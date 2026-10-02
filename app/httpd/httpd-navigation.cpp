/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"
#include <gegl.h>
#include <gtk/gtk.h>
#include "httpd.h"
#include "httpd-private.hpp"
#include "painter/boundary.hpp"
#include "painter/connection.hpp"
extern "C"
{
#include "core/gimp.h"
#include "core/gimpimage.h"
#include "core/gimpitem.h"
#include "core/gimpdrawable.h"
#include "display/display-types.h"
#include "display/gimpdisplay.h"
#include "display/gimpdisplayshell.h"
}
using namespace GimpPainter;
using namespace GimpPainter::Http;
namespace
{
struct GuideState
{
  WeakRef<GObject>        image, item, drawable;
  ObjectRef<GObject>      widget;
  Completion              completion;
  std::vector<Connection> connections;
  bool                    closed = false;
  void
  close () noexcept
  {
    if (closed)
      return;
    closed     = true;
    completion = {};
    auto old   = std::move (connections);
    old.clear ();
    if (widget && ! gtk_widget_in_destruction (GTK_WIDGET (widget.get ())))
      gtk_widget_destroy (GTK_WIDGET (widget.get ()));
    widget.reset ();
  }
  ~GuideState () { close (); }
};
class NativeGuide final : public Guide
{
  std::shared_ptr<GuideState> state_ = std::make_shared<GuideState> ();
  const void                 *window_identity_ = nullptr;

public:
  NativeGuide (Gimp *, const Context &c, const std::string &message, Completion done)
  {
    auto *shell  = gimp_display_get_shell (c.display);
    auto *window = shell ? gimp_display_shell_get_window (shell) : nullptr;
    if (! window || ! shell)
      throw Failure (404, "Display window is closed");
    window_identity_ = window;
    if (c.image && gimp_display_get_image (c.display) != c.image)
      throw Failure (400, "Navigation display and image differ");
    state_->image = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (c.image)));
    state_->item = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (c.item)));
    state_->drawable = WeakRef<GObject> (ObjectRef<GObject>::retain (G_OBJECT (c.drawable)));
    state_->completion = std::move (done);
    auto *dialog       = gtk_message_dialog_new (GTK_WINDOW (window),
                                                 GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_INFO,
                                                 GTK_BUTTONS_OK, "%s", message.c_str ());
    /* Deliberately nonmodal: the user can keep painting/selecting before OK,
     * just as with the pinned 2.8 guide bar. No gtk_dialog_run/nested loop. */
    gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
    gtk_window_set_title (GTK_WINDOW (dialog), "GIMP Painter navigation");
    gtk_widget_set_name (dialog, "gimp-painter-http-navigation");
    state_->widget = ObjectRef<GObject>::sink (G_OBJECT (dialog));
    auto *weak     = new std::weak_ptr<GuideState> (state_);
    g_signal_connect_data (
        dialog, "response", G_CALLBACK (+[] (GtkDialog *, gint response, gpointer data) {
          auto s = static_cast<std::weak_ptr<GuideState> *> (data)->lock ();
          if (! s || s->closed)
            return;
          boundary_void (nullptr, [&] {
            auto complete = std::move (s->completion);
            auto image = s->image.lock (), item = s->item.lock (),
                 drawable = s->drawable.lock ();
            s->close ();
            if (response != GTK_RESPONSE_OK || ! complete)
              return;
            Context c;
            c.image     = image ? GIMP_IMAGE (image.get ()) : nullptr;
            c.item      = item ? GIMP_ITEM (item.get ()) : nullptr;
            c.drawable  = drawable ? GIMP_DRAWABLE (drawable.get ()) : nullptr;
            auto result = object ();
            set (result.get (), "context", context_json (c));
            complete (reply (std::move (result)).body);
          });
        }),
        weak,
        +[] (gpointer p, GClosure *) {
          delete static_cast<std::weak_ptr<GuideState> *> (p);
        },
        GConnectFlags (0));
    auto on_destroy = G_CALLBACK (+[] (GtkWidget *, gpointer data) {
      auto s = static_cast<std::weak_ptr<GuideState> *> (data)->lock ();
      if (s)
        s->close ();
    });
    auto cleanup    = +[] (gpointer p, GClosure *) {
      delete static_cast<std::weak_ptr<GuideState> *> (p);
    };
    for (auto *w : { GTK_WIDGET (shell), dialog })
      {
        auto data = std::unique_ptr<std::weak_ptr<GuideState> > (
            new std::weak_ptr<GuideState> (state_));
        auto connection = Connection::connect (ObjectRef<GObject>::retain (G_OBJECT (w)),
                                               "destroy", on_destroy, data.get (), cleanup);
        data.release ();
        state_->connections.push_back (std::move (connection));
      }
    auto data = std::unique_ptr<std::weak_ptr<GuideState> > (
        new std::weak_ptr<GuideState> (state_));
    auto connection = Connection::connect (
        ObjectRef<GObject>::retain (G_OBJECT (c.display)), "notify::image",
        G_CALLBACK (+[] (GObject *, GParamSpec *, gpointer data) {
          auto s = static_cast<std::weak_ptr<GuideState> *> (data)->lock ();
          if (s)
            s->close ();
        }),
        data.get (), cleanup);
    data.release ();
    state_->connections.push_back (std::move (connection));
    gtk_widget_show (dialog);
  }
  void
  close () noexcept override
  {
    state_->close ();
  }
  const void *
  scope () const noexcept override
  {
    return window_identity_;
  }
  bool
  active () const noexcept override
  {
    return ! state_->closed;
  }
  ~NativeGuide () override { close (); }
};
}
void
gimp_painter_httpd_enable_navigation (GObject *owner)
{
  boundary_void (nullptr, [&] {
    set_guide_factory (owner, [] (Gimp *g, const Context &c,
                                  const std::string &message, Completion done) {
      return std::unique_ptr<Guide> (new NativeGuide (g, c, message, std::move (done)));
    });
  });
}
