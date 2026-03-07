#ifndef GEOS_HELPERS_HPP
#define GEOS_HELPERS_HPP

#ifdef GEOS_VERSION

#include <memory>
#include <geos/geom/Geometry.h>
#include "geometry.hpp"
#include <geos/geom/LinearRing.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/MultiLineString.h>

std::unique_ptr<geos::geom::LineString> to_geos(
    const linestring_type_fp& ls);
std::unique_ptr<geos::geom::LinearRing> to_geos(const ring_type_fp& ring);
std::unique_ptr<geos::geom::Polygon> to_geos(const polygon_type_fp& poly);
std::unique_ptr<geos::geom::MultiPolygon> to_geos(const multi_polygon_type_fp& mpoly);
std::unique_ptr<geos::geom::MultiLineString> to_geos(const multi_linestring_type_fp& mls);

template <typename T>
T from_geos(const std::unique_ptr<geos::geom::Geometry>& g);

linestring_type_fp from_geos(const std::unique_ptr<geos::geom::LineString>& ls);
ring_type_fp from_geos(const std::unique_ptr<geos::geom::LinearRing>& ring);
polygon_type_fp from_geos(const std::unique_ptr<geos::geom::Polygon>& poly);
multi_polygon_type_fp from_geos(const std::unique_ptr<geos::geom::MultiPolygon>& mpoly);
multi_linestring_type_fp from_geos(const std::unique_ptr<geos::geom::MultiLineString>& mls);

#endif // GEOS_VERSION

#endif //GEOS_HELPERS_HPP
