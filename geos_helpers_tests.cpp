#define BOOST_TEST_MODULE geos helpers tests
#include <boost/test/unit_test.hpp>

#include "geometry.hpp"
#include "geos_helpers.hpp"
#include "bg_operators.hpp"
#ifdef GEOS_VERSION
#include <geos/io/WKTReader.h>
#endif // GEOS_VERSION
#include <boost/pointer_cast.hpp>

BOOST_AUTO_TEST_SUITE(geos_helpers_tests)

BOOST_AUTO_TEST_SUITE(boost_geometry)

// Holes are counter-clockwise
BOOST_AUTO_TEST_CASE(polygon_with_holes_direction) {
  auto box = bg::return_envelope<box_type_fp>(point_type_fp(0,0));
  bg::expand(box, point_type_fp(10,10));
  auto hole = bg::return_envelope<box_type_fp>(point_type_fp(3,3));
  bg::expand(hole, point_type_fp(7,7));
  multi_polygon_type_fp mpoly;
  bg::convert(box, mpoly);
  mpoly = mpoly - hole;
  BOOST_CHECK_EQUAL(mpoly[0].outer()[1], point_type_fp(0,10));
  BOOST_CHECK_EQUAL(mpoly[0].inners()[0][1], point_type_fp(7,3));
  bg::reverse(mpoly);
  BOOST_CHECK_EQUAL(mpoly[0].outer()[1], point_type_fp(10,0));
  BOOST_CHECK_EQUAL(mpoly[0].inners()[0][1], point_type_fp(3,7));
  bg::correct(mpoly);
  BOOST_CHECK_EQUAL(mpoly[0].outer()[1], point_type_fp(0,10));
  BOOST_CHECK_EQUAL(mpoly[0].inners()[0][1], point_type_fp(7,3));
}

BOOST_AUTO_TEST_SUITE_END()

#ifdef GEOS_VERSION

BOOST_AUTO_TEST_SUITE(geos_geometry)

BOOST_AUTO_TEST_CASE(polygon_with_holes_direction) {
  // Convert it through well-known text, which is sure to create a
  // valid polygon in geos.
  geos::io::WKTReader reader;
  auto geos_geo = reader.read("MULTIPOLYGON(((0 0,0 10,10 10,10 0,0 0),(3 3,7 3,7 7,3 7,3 3)))");
  auto* geos_poly = dynamic_cast<geos::geom::MultiPolygon*>(geos_geo.get());
  geos_poly->normalize();
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getExteriorRing()->getPointN(1)->getX(), 0);
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getExteriorRing()->getPointN(1)->getY(), 10);
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getInteriorRingN(0)->getPointN(1)->getX(), 7);
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getInteriorRingN(0)->getPointN(1)->getY(), 3);

  // Now convert a reversed version.
  geos_geo = reader.read("MULTIPOLYGON(((0 0,10 0,10 10,0 10,0 0),(3 3,3 7,7 7,7 3,3 3)))");
  geos_poly = dynamic_cast<geos::geom::MultiPolygon*>(geos_geo.get());
  geos_poly->normalize();
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getExteriorRing()->getPointN(1)->getX(), 0);
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getExteriorRing()->getPointN(1)->getY(), 10);
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getInteriorRingN(0)->getPointN(1)->getX(), 7);
  BOOST_CHECK_EQUAL(geos_poly->getGeometryN(0)->getInteriorRingN(0)->getPointN(1)->getY(), 3);
}

BOOST_AUTO_TEST_SUITE(roundtrip)

BOOST_AUTO_TEST_CASE(multi_linestring) {
  multi_linestring_type_fp mls{{{0,0}, {1,1}}, {{2,2},{3,3}}};
  BOOST_CHECK_EQUAL(from_geos(to_geos(mls)), mls);
}

BOOST_AUTO_TEST_CASE(linestring) {
  linestring_type_fp ls{{0,0}, {1,1}};
  BOOST_CHECK_EQUAL(from_geos(to_geos(ls)), ls);
}

BOOST_AUTO_TEST_CASE(polygon) {
  polygon_type_fp poly{{{0,0}, {0,10}, {10,10}, {10,0}, {0,0}}};
  BOOST_CHECK(bg::equals(from_geos(to_geos(poly)), poly));
}

BOOST_AUTO_TEST_CASE(ring) {
  ring_type_fp ring{{0,0}, {0,10}, {10,10}, {10,0}, {0,0}};
  BOOST_CHECK_EQUAL(from_geos(to_geos(ring)), ring);
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(convert_multi_polygon_exception) {
  linestring_type_fp ls{{0,0}, {1,1}};
  auto geos_ls = boost::dynamic_pointer_cast<geos::geom::Geometry>(to_geos(ls));
  BOOST_CHECK_THROW(from_geos<multi_polygon_type_fp>(geos_ls), std::logic_error);
}

BOOST_AUTO_TEST_SUITE_END()

#endif // GEOS_VERSION

BOOST_AUTO_TEST_SUITE_END()
