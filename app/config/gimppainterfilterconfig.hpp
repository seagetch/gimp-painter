/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_FILTER_CONFIG_HPP
#define GIMP_PAINTER_FILTER_CONFIG_HPP
#include <glib-object.h>
#include <memory>
#include "painter/work-admission.hpp"
namespace GimpPainter {
/* Returns owned independent accounting state, never a Config Impl borrow. */
std::shared_ptr<WorkAdmission> filter_admission_for_config (GObject *owner);
}
#endif
