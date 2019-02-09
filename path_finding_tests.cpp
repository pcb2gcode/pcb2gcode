#define BOOST_TEST_MODULE path finding tests
#include <boost/test/included/unit_test.hpp>
#include <boost/optional.hpp>
using boost::make_optional;

#include <boost/optional/optional_io.hpp>

#include "path_finding.hpp"

using namespace std;
using namespace path_finding;

namespace std {

std::ostream& operator<<(std::ostream& out, const linestring_type_fp& ls) {
  out << bg::wkt(ls);
  return out;
}

bool operator==(const point_type_fp& a, const point_type_fp& b) {
  return bg::equals(a, b);
}

} // namespace std

BOOST_AUTO_TEST_SUITE(path_finding_tests);

BOOST_AUTO_TEST_CASE(simple) {
  auto ret = find_path(point_type_fp(0,0), point_type_fp(1,1),
                       boost::none, boost::none, 0.1);

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
  auto ret = find_path(point_type_fp(0,0), point_type_fp(10,10),
                       boost::none, boost::make_optional(keep_out), 0.1);

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
  auto ret = find_path(point_type_fp(0,0), point_type_fp(5,5),
                       boost::none, boost::make_optional(keep_out), 0.1);

  BOOST_CHECK_EQUAL(ret, boost::none);
}

BOOST_AUTO_TEST_SUITE_END()
