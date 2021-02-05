#ifndef GEOS_HELPERS_HPP
#define GEOS_HELPERS_HPP

#ifdef GEOS_VERSION

#include <memory>
#include <geos/geom/Geometry.h>
#include "geometry.hpp"

template <typename T>
std::unique_ptr<geos::geom::Geometry> to_geos(const T& mp);
template <typename T>
T from_geos(const std::unique_ptr<geos::geom::Geometry>& g);

#endif // GEOS_VERSION

#endif //GEOS_HELPERS_HPP
