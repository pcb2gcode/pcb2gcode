#define BOOST_TEST_MODULE path finding tests
#include <boost/test/unit_test.hpp>

#include <ostream>
#include "geometry.hpp"
#include "bg_operators.hpp"
#include "bg_helpers.hpp"

#include <boost/optional.hpp>
using boost::make_optional;

#include <boost/optional/optional_io.hpp>

#include "path_finding.hpp"

using namespace std;
using namespace path_finding;

BOOST_AUTO_TEST_SUITE(path_finding_tests)
BOOST_AUTO_TEST_SUITE(is_intersecting_tests)

// From: https://stackoverflow.com/a/14409947/4454
class Adler32 {
 public:
  void add(char x) {
    s1 = (s1 + x) % 65521;
    s2 = (s2 + s1) % 65521;
  }
  uint32_t get() {
    return (s2 << 16) | s1;
  }

 private:
  uint32_t s1 = 1;
  uint32_t s2 = 0;
};

BOOST_AUTO_TEST_CASE(big) {
  Adler32 hasher;
  for (double x0 = -4; x0 < 4; x0++) {
    for (double y0 = -4; y0 < 4; y0++) {
      for (double x1 = -4; x1 < 4; x1++) {
        for (double y1 = -4; y1 < 4; y1++) {
          for (double x2 = -4; x2 < 4; x2++) {
            for (double y2 = -4; y2 < 4; y2++) {
              for (double x3 = -4; x3 < 4; x3++) {
                for (double y3 = -4; y3 < 4; y3++) {
                  point_type_fp p0{x0, y0};
                  point_type_fp p1{x1, y1};
                  point_type_fp p2{x2, y2};
                  point_type_fp p3{x3, y3};
                  linestring_type_fp ls0{p0,p1};
                  linestring_type_fp ls1{p2,p3};
                  hasher.add(is_intersecting(p0, p1, p2, p3));
                }
              }
            }
          }
        }
      }
    }
  }
  BOOST_CHECK_EQUAL(hasher.get(), 4000232678);
}

// This test is slow but it can be used once to confirm the results of the test above.
BOOST_AUTO_TEST_CASE(small, *boost::unit_test::disabled()) {
  for (double x0 = -3; x0 < 3; x0++) {
    for (double y0 = -3; y0 < 3; y0++) {
      for (double x1 = -3; x1 < 3; x1++) {
        for (double y1 = -3; y1 < 3; y1++) {
          for (double x2 = -3; x2 < 3; x2++) {
            for (double y2 = -3; y2 < 3; y2++) {
              for (double x3 = -3; x3 < 3; x3++) {
                for (double y3 = -3; y3 < 3; y3++) {
                  point_type_fp p0{x0, y0};
                  point_type_fp p1{x1, y1};
                  point_type_fp p2{x2, y2};
                  point_type_fp p3{x3, y3};
                  linestring_type_fp ls0{p0,p1};
                  linestring_type_fp ls1{p2,p3};
                  BOOST_TEST_INFO("intersection: " << bg::wkt(ls0) << " " << bg::wkt(ls1));
                  BOOST_CHECK_EQUAL(
                      path_finding::is_intersecting(p0, p1, p2, p3),
                      bg::intersects(ls0, ls1));
                }
              }
            }
          }
        }
      }
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_CASE(simple) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = create_path_finding_surface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(1,1), nullptr);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_CASE(hole) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-10, -10));
  bg::expand(bounding_box, point_type_fp(10, 10));
  multi_polygon_type_fp keep_out;
  bg::convert(bounding_box, keep_out);
  box_type_fp hole = bg::return_envelope<box_type_fp>(point_type_fp(-5, -5));
  bg::expand(hole, point_type_fp(5, 5));
  multi_polygon_type_fp poly_hole;
  bg::convert(hole, poly_hole);
  keep_out = keep_out - poly_hole;
  auto surface = create_path_finding_surface(boost::none, keep_out, 0.1);
  auto ret = find_path(surface, point_type_fp(0, 0), point_type_fp(1, 1), nullptr);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_CASE(hole_unreachable) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-10, -10));
  bg::expand(bounding_box, point_type_fp(10, 10));
  multi_polygon_type_fp keep_out;
  bg::convert(bounding_box, keep_out);
  box_type_fp hole = bg::return_envelope<box_type_fp>(point_type_fp(-5, -5));
  bg::expand(hole, point_type_fp(5, 5));
  multi_polygon_type_fp poly_hole;
  bg::convert(hole, poly_hole);
  keep_out = keep_out - poly_hole;
  auto surface = create_path_finding_surface(boost::none, keep_out, 0.1);
  auto ret = find_path(surface, point_type_fp(0, 0), point_type_fp(50, 50), nullptr);
  BOOST_CHECK_EQUAL(ret, boost::none);
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
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = create_path_finding_surface(keep_in, keep_out, 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(10,10), nullptr);

  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(3, 7));
  expected.push_back(point_type_fp(10, 10));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_CASE(box_no_keep_in) {
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
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  auto surface = create_path_finding_surface(boost::none, keep_out, 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(10,10), nullptr);

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
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = create_path_finding_surface(keep_in, keep_out, 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(5,5), nullptr);

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
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = create_path_finding_surface(keep_in, keep_out, 0.1);
  auto ret = find_path(surface, point_type_fp(0,0), point_type_fp(5,5), nullptr);
  BOOST_CHECK_EQUAL(ret, boost::none);

  ret = find_path(surface, point_type_fp(0,0), point_type_fp(10,10), nullptr);
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
  auto surface = create_path_finding_surface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = find_path(surface, point_type_fp(1,9), point_type_fp(9,9), nullptr);

  linestring_type_fp expected;
  expected.push_back(point_type_fp(1, 9));
  expected.push_back(point_type_fp(3, 3));
  expected.push_back(point_type_fp(7, 3));
  expected.push_back(point_type_fp(9, 9));
  BOOST_CHECK_EQUAL(ret, make_optional(expected));
}

BOOST_AUTO_TEST_SUITE_END()
