#define BOOST_TEST_MODULE path finding tests

#include <ostream>
#include "geometry.hpp"

namespace std {

std::ostream& operator<<(std::ostream& out, const linestring_type_fp& ls) {
  out << bg::wkt(ls);
  return out;
}

} // namespace std

namespace boost { namespace geometry { namespace model { namespace d2 {

bool operator==(const point_type_fp& a, const point_type_fp& b) {
  return bg::equals(a, b);
}

}}}} // namespace boost::geometry::model::d2

#include <boost/test/included/unit_test.hpp>
#include <boost/optional.hpp>
using boost::make_optional;

#include <boost/optional/optional_io.hpp>

#include "path_finding.hpp"

using namespace std;
using namespace path_finding;

BOOST_AUTO_TEST_SUITE(path_finding_tests);

BOOST_AUTO_TEST_CASE(simple) {
  auto surface = create_path_finding_surface(boost::none, boost::none, 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(1,1));
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_CASE(box) {
  ring_type_fp box;
  box.push_back(point_type_fp(3,3));
  box.push_back(point_type_fp(3,7));
  box.push_back(point_type_fp(7,7));
  box.push_back(point_type_fp(8,3));
  box.push_back(point_type_fp(3,3));
  polygon_type_fp poly;
  poly.outer() = box;
  multi_polygon_type_fp keep_out;
  keep_out.push_back(poly);
  auto surface = create_path_finding_surface(boost::none, boost::make_optional(keep_out), 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(10,10));

  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(3, 7));
  expected.push_back(point_type_fp(10, 10));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_CASE(unreachable_box) {
  ring_type_fp box;
  box.push_back(point_type_fp(3,3));
  box.push_back(point_type_fp(3,7));
  box.push_back(point_type_fp(7,7));
  box.push_back(point_type_fp(8,3));
  box.push_back(point_type_fp(3,3));
  polygon_type_fp poly;
  poly.outer() = box;
  multi_polygon_type_fp keep_out;
  keep_out.push_back(poly);
  auto surface = create_path_finding_surface(boost::none, boost::make_optional(keep_out), 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(5,5));

  BOOST_CHECK_EQUAL(ret, boost::none);
}

BOOST_AUTO_TEST_CASE(reuse_surface) {
  ring_type_fp box;
  box.push_back(point_type_fp(3,3));
  box.push_back(point_type_fp(3,7));
  box.push_back(point_type_fp(7,7));
  box.push_back(point_type_fp(8,3));
  box.push_back(point_type_fp(3,3));
  polygon_type_fp poly;
  poly.outer() = box;
  multi_polygon_type_fp keep_out;
  keep_out.push_back(poly);
  auto surface = create_path_finding_surface(boost::none, boost::make_optional(keep_out), 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(5,5));
  BOOST_CHECK_EQUAL(ret, boost::none);

  ret = find_path(surface, point_type_fp(0,0), point_type_fp(10,10));
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(3, 7));
  expected.push_back(point_type_fp(10, 10));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_CASE(u_shape) {
  ring_type_fp u_shape;
  u_shape.push_back(point_type_fp( 0, 10));
  u_shape.push_back(point_type_fp( 3, 10));
  u_shape.push_back(point_type_fp( 3,  3));
  u_shape.push_back(point_type_fp( 7,  3));
  u_shape.push_back(point_type_fp( 7, 10));
  u_shape.push_back(point_type_fp(10, 10));
  u_shape.push_back(point_type_fp(10,  0));
  u_shape.push_back(point_type_fp( 0,  0));
  u_shape.push_back(point_type_fp( 0, 10));
  polygon_type_fp poly;
  poly.outer() = u_shape;
  multi_polygon_type_fp keep_in;
  keep_in.push_back(poly);
  auto surface = create_path_finding_surface(boost::make_optional(keep_in), boost::none, 0.1);
  auto ret = find_path(surface, point_type_fp(1,9), point_type_fp(9,9));

  linestring_type_fp expected;
  expected.push_back(point_type_fp(1, 9));
  expected.push_back(point_type_fp(3, 3));
  expected.push_back(point_type_fp(7, 3));
  expected.push_back(point_type_fp(9, 9));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_SUITE_END()
