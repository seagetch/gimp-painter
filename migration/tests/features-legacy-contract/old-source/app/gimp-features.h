/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 1995 Spencer Kimball and Peter Mattis
 *
 * Feature
 * Copyright (C) 2017 seagetch <sigetch@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GIMP_FEATURES_H__
#define __GIMP_FEATURES_H__

#ifdef __cplusplus

extern "C" {
#endif

#include "core/core-types.h"
#include "core/gimp.h"

void      features_entry_point       (Gimp* gimp);


#ifdef __cplusplus
}; // extern "C"

namespace GIMP {

class Feature
{
  bool lazy = false;
protected:
  void set_lazy(bool l) { lazy = l; }
  bool get_lazy()       { return lazy; }

  virtual void     on_initialize          (Gimp               *gimp,
                                           GimpInitStatusFunc  status_callback);
  virtual void     on_restore             (Gimp               *gimp,
                                           GimpInitStatusFunc  status_callback);
  virtual gboolean on_exit                (Gimp               *gimp,
                                           gboolean            force);

public:
  Feature() : lazy(false) { };
  virtual ~Feature() { };

  virtual void     entry_point            (Gimp* gimp);
  virtual void     initialize             (Gimp*              gimp, 
                                           GimpInitStatusFunc status_callback) = 0;
  virtual void     restore                (Gimp*              gimp, 
                                           GimpInitStatusFunc status_callback) = 0;
  virtual gboolean exit                   (Gimp               *gimp,
                                           gboolean            force) = 0;

};

}; // namespace Gimp

#endif // __cplusplus

#endif /* __GIMP_FEATURES_H__ */
