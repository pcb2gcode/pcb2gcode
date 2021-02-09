#define BOOST_TEST_MODULE geos helpers tests
#include <boost/test/unit_test.hpp>

#include "geometry.hpp"
#include "geos_helpers.hpp"
#include "bg_operators.hpp"
#ifdef GEOS_VERSION
#include <geos/io/WKTReader.h>
#endif // GEOS_VERSION

BOOST_AUTO_TEST_SUITE(geos_helpers_tests)

// Confirm that both geos and boost support clockwise rings.
BOOST_AUTO_TEST_CASE(direction) {
  auto box = bg::return_envelope<box_type_fp>(point_type_fp(0,0));
  bg::expand(box, point_type_fp(10,10));
  polygon_type_fp poly;
  bg::convert(box, poly);
  BOOST_CHECK_EQUAL(poly.outer()[1], point_type_fp(0,10));
  bg::reverse(poly);
  bg::correct(poly);
  BOOST_CHECK_EQUAL(poly.outer()[1], point_type_fp(0,10));
#ifdef GEOS_VERSION
  // Convert it through well-known text, which is sure to create a
  // valid polygon in geos.
  geos::io::WKTReader reader;
  std::stringstream ss;
  ss << bg::wkt(poly);
  auto geos_geo = reader.read(ss.str());
  const auto* geos_poly = dynamic_cast<geos::geom::Polygon*>(geos_geo.get());
  BOOST_CHECK_EQUAL(geos_poly->getExteriorRing()->getPointN(1)->getX(), 0);
  BOOST_CHECK_EQUAL(geos_poly->getExteriorRing()->getPointN(1)->getY(), 10);
#endif // GEOS_VERSION
}

BOOST_AUTO_TEST_SUITE_END()
