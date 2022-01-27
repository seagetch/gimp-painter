/*
 * popover.cpp
 *
 *  Created on: 2017/03/24
 *      Author: seagetch
 */


#include "base/delegators.hpp"
#include "base/glib-cxx-bridge.hpp"
#include "base/glib-cxx-impl.hpp"
#include "base/glib-cxx-utils.hpp"

extern "C" {
#include "config.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gegl.h>

#include "libgimpwidgets/gimpwidgets.h"

#include "widgets-types.h"

#include "core/gimpcontext.h"
#include "core/gimpcontainer.h"
#include "core/gimpmarshal.h"
#include "core/gimpitem.h"

#include "gimpcontainertreeview.h"
#include "gimpcontainertreestore.h"

#include "gimpviewrenderer.h"

#include "gimp-intl.h"
}

#include "popupper.h"

using namespace GLib;

///////////////////////////////////////////////////////////////////////////////


namespace GLib {

template<> auto g_class_type(const GimpPopover* obj) { return (GimpPopoverClass*)NULL; }
typedef UseCStructs<GtkWindow, GimpPopover> CStructs;

struct Popover : virtual public ImplBase, virtual public PopoverInterface
{
  enum
  {
    CANCEL = 0,
    CONFIRM,
    LAST_SIGNAL
  };
  static guint popover_signals[LAST_SIGNAL];
    gint                 view_border_width;

    GtkWidget           *frame;
    GtkWidget           *view;
public:
  Popover(GObject* obj) : ImplBase(obj) {
    view_border_width = 1;

    frame       = gtk_frame_new (NULL);
    auto iframe = ref(frame);
    iframe [gtk_frame_set_shadow_type](GTK_SHADOW_OUT);
    ref(g_object)[gtk_container_add] (frame);
    iframe [gtk_widget_show] ();
  };

  virtual ~Popover() { }
  static void      class_init           (CStructs::Class *klass);

  virtual void     map                  ();
  virtual void     cancel               ();
  virtual void     confirm              ();
  virtual void     close                ();
  virtual void     show                 (GdkScreen *screen,
                                         gint targetLeft,
                                         gint targetTop,
                                         gint targetRight,
                                         gint targetBottom,
                                         GtkCornerType pos);
  virtual void     show_over            (GtkWidget *widget, GdkRectangle* cell_area);
  virtual void     set_view             (GtkWidget *view);

  // Default event handlers
  virtual gboolean button_press_event   (GdkEventButton     *bevent);
  virtual gboolean key_press_event      (GdkEventKey        *kevent);

  // Event handlers
  void             on_grab_notify       (GimpPopover* popover, gboolean   was_grabbed);
  gboolean         on_grab_broken_event (GimpPopover* popover, GdkEventGrabBroken *event);
};

guint Popover::popover_signals[LAST_SIGNAL];

extern const char gimp_popover_name[] = "GimpPopover2";
typedef NewGClass<gimp_popover_name, CStructs, Popover> Class;


#define bind_to_class(klass, method, impl)  Class::__(&klass->method).bind<&impl::method>()

void GLib::Popover::class_init(CStructs::Class *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GtkBindingSet  *binding_set;

  popover_signals[CANCEL] =
    g_signal_new ("cancel",
                  G_OBJECT_CLASS_TYPE (klass),
                  (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                  G_STRUCT_OFFSET (Class::Class, cancel),
                  NULL, NULL,
                  gimp_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  popover_signals[CONFIRM] =
    g_signal_new ("confirm",
                  G_OBJECT_CLASS_TYPE (klass),
                  (GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
                  G_STRUCT_OFFSET (Class::Class, confirm),
                  NULL, NULL,
                  gimp_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
  bind_to_class (widget_class, map               , Popover);
  bind_to_class (widget_class, button_press_event, Popover);
  bind_to_class (widget_class, key_press_event   , Popover);
  bind_to_class (klass, cancel                   , Popover);
  bind_to_class (klass, confirm                  , Popover);
  binding_set = gtk_binding_set_by_class (klass);

  gtk_binding_entry_add_signal (binding_set, GDK_Escape, GdkModifierType(0),
                                "cancel", 0);
/*
  gtk_binding_entry_add_signal (binding_set, GDK_Return, 0,
                                "confirm", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Enter, 0,
                                "confirm", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_ISO_Enter, 0,
                                "confirm", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_space, 0,
                                "confirm", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Space, 0,
                                "confirm", 0);
*/
}

}; // namespace


void GLib::Popover::on_grab_notify (GimpPopover* popover, gboolean   was_grabbed)
{
  if (was_grabbed)
    return;

  /* ignore grabs on one of our children, like the scrollbar */
  if (gtk_widget_is_ancestor (gtk_grab_get_current (), GTK_WIDGET(g_object)))
    return;

/*  g_signal_emit (widget, popover_signals[CANCEL], 0); */
}

gboolean GLib::Popover::on_grab_broken_event (GimpPopover* popover, GdkEventGrabBroken *event)
{
  on_grab_notify(popover, FALSE);
  return FALSE;
}

void GLib::Popover::map ()
{
  auto widget = ref(GTK_WIDGET(g_object));
  GTK_WIDGET_CLASS (Class::parent_class)->map (widget.ptr());

  /*  grab with owner_events == TRUE so the popover's widgets can
   *  receive events. we filter away events outside this toplevel
   *  away in button_press()
   */
  widget[gtk_grab_add]();

  g_signal_connect_delegator (g_object, "grab-notify",
                              Delegators::delegator(this, &Popover::on_grab_notify));
  g_signal_connect_delegator (g_object, "grab-broken-event",
                              Delegators::delegator(this, &Popover::on_grab_broken_event));
  return;
}

gboolean GLib::Popover::button_press_event (GdkEventButton *bevent)
{
  GtkWidget *event_widget;
  gboolean   cancel = FALSE;

  event_widget = gtk_get_event_widget ((GdkEvent *) bevent);

  if (event_widget == GTK_WIDGET(g_object)) {
      GtkAllocation allocation;

      ref(g_object)[gtk_widget_get_allocation](&allocation);

      /*  the event was on the popover, which can either be really on the
       *  popover or outside gimp (owner_events == TRUE, see map())
       */
      if (bevent->x < 0                || bevent->y < 0 ||
          bevent->x > allocation.width || bevent->y > allocation.height) {
        /*  the event was outsde gimp  */
        cancel = TRUE;
      }
    }
  else if (gtk_widget_get_toplevel (event_widget) != GTK_WIDGET(g_object)) {
    /* GtkWidget *parent; */
    /*  the event was on a gimp widget, but not inside the popover  */

    cancel = TRUE;
  }

  if (cancel)
    g_signal_emit (g_object, popover_signals[CANCEL], 0);

  return cancel;
}

gboolean GLib::Popover::key_press_event (GdkEventKey *kevent)
{
  GtkBindingSet  *binding_set = gtk_binding_set_by_class (Class::Traits::get_class(g_object));

  /*  invoke the popover's binding entries manually, because otherwise
   *  the focus widget (GtkTreeView e.g.) would consume it
   */
  if (gtk_binding_set_activate (binding_set, kevent->keyval,
                                GdkModifierType(kevent->state),
                                GTK_OBJECT(g_object))) {
    return TRUE;
  }

  return GTK_WIDGET_CLASS (Class::parent_class)->key_press_event (GTK_WIDGET(g_object), kevent);
}

void GLib::Popover::cancel ()
{
  auto widget = ref(GTK_WIDGET (g_object));

  if (gtk_grab_get_current () == widget.ptr())
    widget[gtk_grab_remove] ();

  widget[gtk_widget_destroy] ();
}

void GLib::Popover::confirm () {
  close ();
}


void GLib::Popover::close () {
  auto widget = ref(GTK_WIDGET (g_object));

  if (gtk_grab_get_current () == widget.ptr())
    widget[gtk_grab_remove] ();

  widget[gtk_widget_destroy] ();
//  g_object_unref (G_OBJECT (widget));
}


void GLib::Popover::show (GdkScreen *screen,
                          gint targetLeft,
                          gint targetTop,
                          gint targetRight,
                          gint targetBottom,
                          GtkCornerType pos)
{
  GtkRequisition  requisition;
  GdkRectangle    rect;
  gint            monitor;
  gint            x, y;
  auto            self = ref(g_object);

  g_return_if_fail (GDK_IS_SCREEN (screen));
  self[gtk_widget_size_request] (&requisition);

  monitor = ref(screen)[gdk_screen_get_monitor_at_point] (targetLeft, targetTop);
  ref(screen)[gdk_screen_get_monitor_geometry] (monitor, &rect);

  x = targetLeft;
  if (pos == GTK_CORNER_TOP_RIGHT || pos == GTK_CORNER_BOTTOM_RIGHT) {
    x = targetRight - requisition.width;

    if (x < rect.x)
      x = targetLeft;
  } else {
    x = targetLeft;

    if (x + requisition.width > rect.x + rect.width)
      x = targetRight - requisition.width;
  }

  if (pos == GTK_CORNER_BOTTOM_LEFT || pos == GTK_CORNER_BOTTOM_RIGHT) {
    y = targetBottom;

    if (y + requisition.height > rect.y + rect.height)
      y = targetTop - requisition.height;
  } else {
    y = targetTop - requisition.height;

    if (y < rect.y)
      y = targetBottom;
  }

  self[gtk_window_move] (x, y);
  self[gtk_widget_show] ();
}

void GLib::Popover::show_over(GtkWidget *widget, GdkRectangle* cell_area)
{
  GdkScreen      *screen;
  GtkRequisition  requisition;
  GtkAllocation   allocation;
  gint            orig_x;
  gint            orig_y;
  auto            self    = ref(g_object);
  auto            _widget = ref(widget);

  g_return_if_fail (GTK_IS_WIDGET (widget));

  self[gtk_widget_size_request] (&requisition);

  _widget[gtk_widget_get_allocation] (&allocation);
  ref( _widget[gtk_widget_get_window]() ) [gdk_window_get_origin] (&orig_x, &orig_y);

  if (! _widget[gtk_widget_get_has_window] ())
    {
      orig_x += allocation.x;
      orig_y += allocation.y;
    }

  screen = _widget[gtk_widget_get_screen] ();

  if (cell_area)
    show (screen,
          orig_x + cell_area->x,                    orig_y + cell_area->y,
          orig_x + cell_area->x + cell_area->width, orig_y + cell_area->y + cell_area->height,
          GTK_CORNER_BOTTOM_LEFT);
  else
    show (screen, orig_x, orig_y, orig_x + allocation.width, orig_y + allocation.height,
          GTK_CORNER_BOTTOM_LEFT);
}

void GLib::Popover::set_view (GtkWidget *view)
{
  this->view = view;

  ref(frame)[gtk_container_add] (GTK_WIDGET (view));
  ref(view) [gtk_widget_show] ();

  ref(view) [gtk_widget_grab_focus] ();
}
/*  private functions  */
/* no private functions */

GtkWidget* PopoverInterface::new_instance (GtkWidget *view)
{
  GtkWidget* widget;
  GLib::Popover *popover;

  widget = GTK_WIDGET(g_object_new (GLib::Class::get_type(),
                        "type", GTK_WINDOW_POPUP,
                        NULL));
  ref(widget)[gtk_window_set_resizable] (FALSE);

  popover = dynamic_cast<GLib::Popover*>(PopoverInterface::cast(widget));
  popover->view_border_width = 0;

  popover->set_view (view);

  return GTK_WIDGET (widget);
}

PopoverInterface*
PopoverInterface::cast(gpointer obj) {
  return dynamic_cast<PopoverInterface*>(GLib::Class::get_private(obj));
}

bool PopoverInterface::is_instance(gpointer obj) {
  return GLib::Class::Traits::is_instance(obj);
}

///////////////////////////////////////////////////////////////////////////////
extern "C" {
GtkWidget* gimp_popover_new (GtkWidget*view)
{
  return PopoverInterface::new_instance (view);
}
}; // extern "C"
///////////////////////////////////////////////////////////////////////////////

class PopoverDecorator {
public:
private:
  CreateViewDelegator* create_view_delegator;
  Delegators::Connection* cancel_handler;
  Delegators::Connection* confirm_handler;
  Delegators::Connection* scroll_event_handler;
  Delegators::Connection* button_press_handler;
public:
  gboolean scroll_event (GtkWidget *widget, GdkEventScroll *sevent) { return TRUE; };

  void on_popover_closed (GimpPopover *popover) {
    // Handlers are already deleted by popover.
    cancel_handler  = NULL;
    confirm_handler = NULL;
  }

  gboolean button_press (GtkWidget      *widget,
                         GdkEventButton *bevent) {
    clicked_internal(widget, NULL);
    return FALSE;
  }

  void parent_clicked(GObject* object, GObject* widget, const gchar* path_str, GdkRectangle* cell_area) {
    g_print("Popover::parent_clicked:%s:%s\n", (const gchar*)ref(widget)["name"], path_str);
    GtkTreeView*  tree_view = GTK_TREE_VIEW(widget);
    GtkTreeModel* model;
    GtkTreePath*  path;
    GtkTreeIter   iter;
    path  = gtk_tree_path_new_from_string(path_str);
    model = gtk_tree_view_get_model(tree_view);

    if (gtk_tree_model_get_iter (model, &iter, path)) {
      GimpViewRenderer *renderer;
      GimpItem         *item;
      gboolean          active;

      gtk_tree_model_get (model, &iter,
                          GIMP_CONTAINER_TREE_STORE_COLUMN_RENDERER, &renderer,
                          -1);
      item = GIMP_ITEM (renderer->viewable);
      g_object_unref (renderer);

      clicked_internal(GTK_WIDGET(widget), cell_area, item);
    }
  }

  void clicked(GtkWidget* widget) {
    clicked_internal(widget, NULL);
  }

  void clicked_internal(GtkWidget* widget, GdkRectangle* cell_area, gpointer data = NULL) {
    GtkWidget       *popover_widget;
    PopoverInterface  *popover;
    GtkWidget       *view;
    g_print("Popover::clicked: %p(%s)\n", widget, G_OBJECT_TYPE_NAME(widget));

    view = create_view (widget, data);

    g_return_if_fail (GTK_IS_WIDGET (view));

    popover_widget = PopoverInterface::new_instance (view);
    popover        = PopoverInterface::cast(popover_widget);

    if (cancel_handler) {
      delete cancel_handler;
      cancel_handler = NULL;
    }
    if (confirm_handler) {
      delete confirm_handler;
      confirm_handler = NULL;
    }

    cancel_handler =
      g_signal_connect_delegator (G_OBJECT(popover_widget), "cancel", delegator(this, &PopoverDecorator::on_popover_closed));
    confirm_handler =
      g_signal_connect_delegator (G_OBJECT(popover_widget), "confirm", delegator(this, &PopoverDecorator::on_popover_closed));

    popover->show_over (widget, cell_area);
  }

  GtkWidget* create_view (GtkWidget* widget, gpointer aux) {
    g_print("Popover::create_view\n");
    GtkWidget *result = NULL;
    if (create_view_delegator)
      (*create_view_delegator)(widget, &result, aux);
    else
      g_print("Error: PopoverDecorator::create_view: create_view_delegator is NULL.\n");

    g_return_val_if_fail (GTK_IS_WIDGET (result), NULL);

    return result;
  }

  PopoverDecorator(GObject* widget, CreateViewDelegator* delegator) {
    g_print("PopoverDecorator::PopoverDecorator\n");
    create_view_delegator = delegator;
    cancel_handler = NULL;
    confirm_handler = NULL;
    scroll_event_handler =
      g_signal_connect_delegator (G_OBJECT(widget), "scroll-event", Delegators::delegator(this, &PopoverDecorator::scroll_event));
    button_press_handler =
      g_signal_connect_delegator (G_OBJECT(widget), "button-press-event", Delegators::delegator(this, &PopoverDecorator::button_press));
    if (!button_press_handler->is_valid())
      button_press_handler =
          g_signal_connect_delegator (G_OBJECT(widget), "clicked", Delegators::delegator(this, &PopoverDecorator::clicked));
    if (!button_press_handler->is_valid())
      button_press_handler =
          g_signal_connect_delegator (G_OBJECT(widget), "parent-clicked", Delegators::delegator(this, &PopoverDecorator::parent_clicked));
  }

  ~PopoverDecorator() {
    if (cancel_handler)
      delete cancel_handler;
    if (confirm_handler)
      delete confirm_handler;
    if (create_view_delegator)
      delete create_view_delegator;
  }

};

///////////////////////////////////////////////////////////////////////////////
void decorate_popover(GObject* widget, CreateViewDelegator* delegator)
{
  PopoverDecorator* popover = new PopoverDecorator(widget, delegator);
  decorator(widget, popover);
}

///////////////////////////////////////////////////////////////////////////////
class ToolbarPopoverViewCreator {
private:
  IObject<GObject>              config;
  GimpPopoverCreateViewCallbackExt create_view_full_handler;
  GimpPopoverCreateViewCallback    create_view_handler;
  gpointer                       data;
  void (*data_destructor) (gpointer data);
public:
  ToolbarPopoverViewCreator(GObject* c,
                          GimpPopoverCreateViewCallbackExt cv,
                          gpointer d,
                          void(*dd)(gpointer))
  : config(c), create_view_handler(NULL), create_view_full_handler(NULL),
    data(NULL), data_destructor(NULL) {
    create_view_full_handler = cv;
    data = d;
    data_destructor = dd;
  }
  ToolbarPopoverViewCreator(GObject* c,
                          GimpPopoverCreateViewCallback cv)
  : config(c), create_view_handler(NULL), create_view_full_handler(NULL),
    data(NULL), data_destructor(NULL) {
    create_view_handler = cv;
  }
  ~ToolbarPopoverViewCreator() {
    if (data && data_destructor)
      (*data_destructor)(data);
  }
  void create_view(GtkWidget* widget, GtkWidget** result) {
    if (create_view_full_handler) {
      (*create_view_full_handler)(widget, result,config.ptr(), data);
    } else if (create_view_handler) {
      (*create_view_handler)(widget, result, config.ptr());
    }
  }
};
