/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef PAINTER_TEST_FIXTURE_TRAITS_HPP
#define PAINTER_TEST_FIXTURE_TRAITS_HPP

#include "test-fixture.h"
#include "object-ref.hpp"

namespace GimpPainter {
template<> struct TypeTraits<PainterFixture>
{ static GType type () noexcept { return painter_fixture_get_type (); } };
}

#endif
