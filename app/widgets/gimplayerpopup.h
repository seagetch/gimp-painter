/*
 * gimplayerpopup.h
 *
 *  Created on: 2022/01/26
 *      Author: seagetch
 */

#ifndef APP_WIDGETS_LAYER_POPUP_H_
#define APP_WIDGETS_LAYER_POPUP_H_

#ifdef __cplusplus
class LayerPopupWindow {
public:
  LayerPopupWindow();
  ~LayerPopupWindow();
  void create_view(GtkWidget* widget, GtkWidget** result, gpointer data);
};
#endif

#endif /* APP_WIDGETS_LAYER_POPUP_H_ */