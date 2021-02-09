#ifdef GEOS_VERSION

#include "geos_helpers.hpp"
#include "geometry.hpp"
#include <geos/io/WKTReader.h>
#include <geos/io/WKTReader.inl>
#include <geos/io/WKTWriter.h>
#include <geos/geom/CoordinateSequenceFactory.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/Coordinate.inl>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/GeometryFactory.inl>
#include <boost/pointer_cast.hpp>

linestring_type_fp from_geos(const geos::geom::LineString* ls) {
  linestring_type_fp ret;
  ret.reserve(ls->getNumPoints());
  for (size_t i = 0; i < ls->getNumPoints(); i++) {
    const auto& p = ls->getPointN(i);
    ret.push_back(point_type_fp(p->getX(), p->getY()));
  }
  return ret;
}

linestring_type_fp from_geos(const std::unique_ptr<geos::geom::LineString>& ls) {
  return from_geos(ls.get());
}

ring_type_fp from_geos(const geos::geom::LinearRing* ring) {
  ring_type_fp ret;
  ret.reserve(ring->getNumPoints());
  for (size_t i = 0; i < ring->getNumPoints(); i++) {
    const auto& p = ring->getPointN(i);
    ret.push_back(point_type_fp(p->getX(), p->getY()));
  }
  return ret;
}

ring_type_fp from_geos(const std::unique_ptr<geos::geom::LinearRing>& ring) {
  return from_geos(ring.get());
}

polygon_type_fp from_geos(const geos::geom::Polygon* poly) {
  polygon_type_fp ret;
  ret.outer() = from_geos(poly->getExteriorRing());
  ret.inners().reserve(poly->getNumInteriorRing());
  for (size_t i = 0; i < poly->getNumInteriorRing(); i++) {
    ret.inners().push_back(from_geos(poly->getInteriorRingN(i)));
  }
  return ret;
}

polygon_type_fp from_geos(const std::unique_ptr<geos::geom::Polygon>& poly) {
  return from_geos(poly.get());
}

multi_polygon_type_fp from_geos(const std::unique_ptr<geos::geom::MultiPolygon>& mpoly) {
  multi_polygon_type_fp ret;
  ret.reserve(mpoly->getNumGeometries());
  for (size_t i = 0; i < mpoly->getNumGeometries(); i++) {
    ret.push_back(from_geos(mpoly->getGeometryN(i)));
  }
  return ret;
}

multi_linestring_type_fp from_geos(const std::unique_ptr<geos::geom::MultiLineString>& mls) {
  multi_linestring_type_fp ret;
  ret.reserve(mls->getNumGeometries());
  for (size_t i = 0; i < mls->getNumGeometries(); i++) {
    ret.push_back(from_geos(mls->getGeometryN(i)));
  }
  return ret;
}

template <>
multi_polygon_type_fp from_geos(const std::unique_ptr<geos::geom::Geometry>& g) {
  auto mp = boost::dynamic_pointer_cast<geos::geom::MultiPolygon>(g->clone());
  if (mp) {
    return from_geos(mp);
  }
  auto p = boost::dynamic_pointer_cast<geos::geom::Polygon>(g->clone());
  if (p) {
    return multi_polygon_type_fp{from_geos(p)};
  }
  geos::io::WKTWriter writer;
  throw std::logic_error("Can't convert to multi_polygon_type_fp: " + writer.write(g.get()));
}

std::unique_ptr<geos::geom::LineString> to_geos(
    const linestring_type_fp& ls) {
  auto geom_factory = geos::geom::GeometryFactory::create();
  auto coord_factory = geom_factory->getCoordinateSequenceFactory();
  auto coords = coord_factory->create(ls.size(), 2 /* dimensions */);
  for (size_t i = 0; i < ls.size(); i++) {
    coords->setAt(geos::geom::Coordinate(ls[i].x(), ls[i].y()), i);
  }
  return geom_factory->createLineString(std::move(coords));
}

std::unique_ptr<geos::geom::LinearRing> to_geos(const ring_type_fp& ring) {
  auto geom_factory = geos::geom::GeometryFactory::create();
  auto coord_factory = geom_factory->getCoordinateSequenceFactory();
  auto coords = coord_factory->create(ring.size(), 2 /* dimensions */);
  for (size_t i = 0; i < ring.size(); i++) {
    // reverse rings for geos.
    coords->setAt(geos::geom::Coordinate(ring[i].x(), ring[i].y()), i);
  }
  return geom_factory->createLinearRing(std::move(coords));
}

std::unique_ptr<geos::geom::Polygon> to_geos(const polygon_type_fp& poly) {
  auto shell = to_geos(poly.outer());
  std::vector<std::unique_ptr<geos::geom::LinearRing>> holes;
  holes.reserve(poly.inners().size());
  for (const auto& inner : poly.inners()) {
    holes.push_back(to_geos(inner));
  }
  auto geom_factory = geos::geom::GeometryFactory::create();
  return geom_factory->createPolygon(std::move(shell), std::move(holes));
}

std::unique_ptr<geos::geom::MultiPolygon> to_geos(const multi_polygon_type_fp& mpoly) {
  std::vector<std::unique_ptr<geos::geom::Polygon>> polys;
  polys.reserve(mpoly.size());
  for (const auto& p : mpoly) {
    polys.push_back(to_geos(p));
  }
  auto geom_factory = geos::geom::GeometryFactory::create();
  return geom_factory->createMultiPolygon(std::move(polys));
}

std::unique_ptr<geos::geom::MultiLineString> to_geos(const multi_linestring_type_fp& mls) {
  std::vector<std::unique_ptr<geos::geom::LineString>> polys;
  polys.reserve(mls.size());
  for (const auto& p : mls) {
    polys.push_back(to_geos(p));
  }
  auto geom_factory = geos::geom::GeometryFactory::create();
  return geom_factory->createMultiLineString(std::move(polys));
}

#endif //GEOS_VERSION
