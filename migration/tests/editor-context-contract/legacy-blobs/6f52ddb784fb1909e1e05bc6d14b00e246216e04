/*
 * gimpmypaintbrusheditor.hpp
 *
 * Copyright (C) 2012 - seagetch
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
class MypaintBrushEditorPrivate : public MypaintGUIPrivateBase {
private:
  PageRemindAction* page_reminder;

public:
  enum EditorType { EXPANDER, TAB };
  EditorType type;
  
public:
  MypaintBrushEditorPrivate(GimpToolOptions* opts, EditorType _type = EXPANDER) :
    MypaintGUIPrivateBase(opts), type(_type), page_reminder(NULL)
  {
  }
  GtkWidget* create();
  GtkWidget* create_input_editor(const gchar* prop_name);
  void set_page_reminder(PageRemindAction* reminder) {
    page_reminder = reminder;
  }
};
