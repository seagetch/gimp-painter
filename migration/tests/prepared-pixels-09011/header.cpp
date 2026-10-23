#include <type_traits>
#include "paint/painter-mypaint-surface/paint-core-pixels.hpp"
#include "paint/painter-mypaint-surface/paint-core-pixels.hpp"
static_assert(!std::is_copy_constructible<GimpPainter::MyPaint::PreparedPixels>::value,"owned payload cannot alias a copy");
