/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef GIMP_PAINTER_HTTPD_PRIVATE_HPP
#define GIMP_PAINTER_HTTPD_PRIVATE_HPP
#include "httpd-resource.hpp"
#include "painter/object-ref.hpp"
#include <functional>
namespace GimpPainter
{
namespace Http
{
using Completion = std::function<void (std::string)>;
/* Completion is abandoned when the owner closes. close is called once. */
struct Guide
{
  virtual ~Guide ()                            = default;
  virtual void        close () noexcept        = 0;
  virtual const void *scope () const noexcept  = 0;
  virtual bool        active () const noexcept = 0;
};
using GuideFactory =
    std::function<std::unique_ptr<Guide> (Gimp *, const Context &, const std::string &, Completion)>;
guint pending_count (GObject *);
void  set_guide_factory (GObject *, GuideFactory);
}
}
#endif
