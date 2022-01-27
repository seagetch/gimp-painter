/*
 * popoverper.h
 *
 *  Created on: 2017/03/24
 *      Author: seagetch
 */

#ifndef APP_WIDGETS_POPOVERPER_H_
#define APP_WIDGETS_POPOVERPER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GimpPopoverCreateViewCallback) (GtkWidget *button, GtkWidget** result, gpointer data);
typedef void (*GimpPopoverCreateViewCallbackExt) (GtkWidget *button, GtkWidget** result, GObject *config, gpointer data);

typedef struct _GimpPopoverClass  GimpPopoverClass;
typedef struct _GimpPopover       GimpPopover;

struct _GimpPopover
{
  GtkWindow            parent_instance;
};

struct _GimpPopoverClass
{
  GtkWindowClass  parent_instance;

  void (* cancel)  (GimpPopover *popover);
  void (* confirm) (GimpPopover *popover);
};

GtkWidget* gimp_popover_new (GtkWidget*view);


#ifdef __cplusplus
}; // extern "C"

class PopoverInterface {
public:
  static GtkWidget*      new_instance   (GtkWidget* widget);
  static PopoverInterface* cast           (gpointer obj);
  static bool            is_instance    (gpointer obj);
  virtual void     show                 (GdkScreen *screen,
                                         gint targetLeft,
                                         gint targetTop,
                                         gint targetRight,
                                         gint targetBottom,
                                         GtkCornerType pos) = 0;
  virtual void     show_over            (GtkWidget *widget, GdkRectangle* cell_area) = 0;
  virtual void     set_view             (GtkWidget *view) = 0;
  virtual gboolean button_press_event   (GdkEventButton     *bevent) = 0;
  virtual gboolean key_press_event      (GdkEventKey        *kevent) = 0;
};
typedef Delegators::Delegator<void(GtkWidget*, GtkWidget**, gpointer)> CreateViewDelegator;
void decorate_popover(GObject* widget, CreateViewDelegator* delegator);

#endif

#endif /* APP_WIDGETS_POPOVERPER_H_ */
