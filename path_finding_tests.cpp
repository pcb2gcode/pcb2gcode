#define BOOST_TEST_MODULE path finding tests
#include <boost/test/unit_test.hpp>

#include <ostream>
#include "geometry.hpp"
#include "bg_operators.hpp"
#include "bg_helpers.hpp"

#include <boost/optional.hpp>

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

BOOST_AUTO_TEST_CASE(point_in_ring_tests) {
  ring_type_fp ring{
    point_type_fp{0,0},
    point_type_fp{0,5},
    point_type_fp{0,10},
    point_type_fp{5,10},
    point_type_fp{10,10},
    point_type_fp{10,0},
    point_type_fp{0,0},
  };
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{5,5}, ring), true);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{0,-1}, ring), false);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{-1,0}, ring), false);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{0,11}, ring), false);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{-1,10}, ring), false);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{-1,5}, ring), false);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{1,5}, ring), true);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{9,5}, ring), true);
  BOOST_CHECK_EQUAL(point_in_ring(point_type_fp{11,5}, ring), false);
}

BOOST_AUTO_TEST_SUITE(inside_multipolygon_tests)

BOOST_AUTO_TEST_CASE(open_space) {
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{1,1}, multi_polygon_type_fp()), boost::none);
}

BOOST_AUTO_TEST_CASE(box) {
  multi_polygon_type_fp mp;
  box_type_fp box{point_type_fp{0,0}, {10,10}};
  bg::convert(box, mp);
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{1,1}, mp), boost::make_optional(MPRingIndices({{0, {0}}})));
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{11,11}, mp), boost::none);
}

BOOST_AUTO_TEST_CASE(doughnuts) {
  multi_polygon_type_fp mp;
  box_type_fp box{{0,0}, {10,10}};
  bg::convert(box, mp);
  box_type_fp hole = {{3,3}, {7,7}};
  multi_polygon_type_fp hole_mp;
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  box = {{20,0}, {30,10}};
  multi_polygon_type_fp second_doughtnut;
  bg::convert(box, second_doughtnut);
  mp = mp + second_doughtnut;
  hole = {point_type_fp{23,3}, {24,4}};
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  hole = {point_type_fp{26,6}, {28,8}};
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;

  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{1,1}, mp), boost::make_optional(MPRingIndices({{0, {0, 1}}})));
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{11,11}, mp), boost::none);
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{5,5}, mp), boost::none);
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{21,1}, mp), boost::make_optional(MPRingIndices({{1, {0, 1, 2}}})));
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{23.5,3.5}, mp), boost::none);
}

BOOST_AUTO_TEST_CASE(nested_doughnuts) {
  multi_polygon_type_fp mp;
  box_type_fp box{point_type_fp{0,0}, {100,100}};
  bg::convert(box, mp);
  box_type_fp hole = {point_type_fp{10,10}, {90,90}};
  multi_polygon_type_fp hole_mp;
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  box = {point_type_fp{20,20}, point_type_fp{80,80}};
  multi_polygon_type_fp second_doughtnut;
  bg::convert(box, second_doughtnut);
  mp = mp + second_doughtnut;
  hole = {point_type_fp{30,30}, {70,70}};
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{1,1}, mp), boost::make_optional(MPRingIndices({{0, {0, 1}}})));
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{11,11}, mp), boost::none);
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{21,21}, mp), boost::make_optional(MPRingIndices({{1, {0, 1}}})));
  BOOST_CHECK_EQUAL(inside_multipolygon(point_type_fp{31,31}, mp), boost::none);
}

BOOST_AUTO_TEST_SUITE_END() // inside_multipolygon_tests

BOOST_AUTO_TEST_SUITE(outside_multipolygon_tests)

BOOST_AUTO_TEST_CASE(open_space) {
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{1,1}, multi_polygon_type_fp()), MPRingIndices());
}

BOOST_AUTO_TEST_CASE(box) {
  multi_polygon_type_fp mp;
  box_type_fp box{point_type_fp{0,0}, {10,10}};
  bg::convert(box, mp);
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{1,1}, mp), boost::none);
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{11,11}, mp), boost::make_optional(MPRingIndices({{0, {0}}})));
}

BOOST_AUTO_TEST_CASE(doughnuts) {
  multi_polygon_type_fp mp;
  box_type_fp box{point_type_fp{0,0}, {10,10}};
  bg::convert(box, mp);
  box_type_fp hole = {point_type_fp{3,3}, {7,7}};
  multi_polygon_type_fp hole_mp;
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  box = {point_type_fp{20,0}, point_type_fp{30,10}};
  multi_polygon_type_fp second_doughtnut;
  bg::convert(box, second_doughtnut);
  mp = mp + second_doughtnut;
  hole = {point_type_fp{23,3}, {24,4}};
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  hole = {point_type_fp{26,6}, {28,8}};
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;

  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{1,1}, mp), boost::none);
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{11,11}, mp), boost::make_optional(MPRingIndices({{0, {0}}, {1, {0}}})));
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{5,5}, mp), boost::make_optional(MPRingIndices({{0, {1}}, {1, {0}}})));
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{21,1}, mp), boost::none);
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{23.5,3.5}, mp), boost::make_optional(MPRingIndices({{0, {0}}, {1, {1}}})));
}

BOOST_AUTO_TEST_CASE(nested_doughnuts) {
  multi_polygon_type_fp mp;
  box_type_fp box{point_type_fp{0,0}, {100,100}};
  bg::convert(box, mp);
  box_type_fp hole = {point_type_fp{10,10}, {90,90}};
  multi_polygon_type_fp hole_mp;
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;
  box = {point_type_fp{20,20}, point_type_fp{80,80}};
  multi_polygon_type_fp second_doughtnut;
  bg::convert(box, second_doughtnut);
  mp = mp + second_doughtnut;
  hole = {point_type_fp{30,30}, {70,70}};
  bg::convert(hole, hole_mp);
  mp = mp - hole_mp;

  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{1,1}, mp), boost::none);
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{11,11}, mp), boost::make_optional(MPRingIndices({{0, {1}}, {1, {0}}})));
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{21,21}, mp), boost::none);
  BOOST_CHECK_EQUAL(outside_multipolygon(point_type_fp{31,31}, mp), boost::make_optional(MPRingIndices({{0, {1}}, {1, {1}}})));
}

BOOST_AUTO_TEST_SUITE_END() // outside_multipolygon_tests

BOOST_AUTO_TEST_SUITE(nested_multipolygon_type_fp)

BOOST_AUTO_TEST_CASE(open_space) {
  auto surface = PathFindingSurface(boost::none, multi_polygon_type_fp(), 5);
  BOOST_CHECK_EQUAL(surface.in_surface({1,1}), boost::make_optional(size_t(0)));
}

BOOST_AUTO_TEST_CASE(barbell) {
  multi_polygon_type_fp barbell{{{{0,0}, {0,100}, {40,100}, {40,2}, {60,2},
                                  {60,100}, {100,100}, {100,0}, {0,0}}}};
  auto surface = PathFindingSurface(boost::none, barbell, 5);
  BOOST_CHECK_EQUAL(surface.in_surface({1,1}), boost::make_optional(size_t(0)));
  BOOST_CHECK_EQUAL(surface.in_surface({6,6}), boost::none);
  BOOST_CHECK_EQUAL(surface.in_surface({-10,-10}), boost::make_optional(size_t(0)));
  BOOST_CHECK_EQUAL(surface.in_surface({10,10}), boost::none);
}

BOOST_AUTO_TEST_CASE(almost_doughnut) {
  multi_polygon_type_fp almost_doughnut{{{{0,0}, {0,100}, {49,100}, {49,80},
                                          {20,80}, {20,20}, {80,20}, {80,80},
                                          {51,80}, {51,100}, {100,100},
                                          {100,0}, {0,0}}}};
  auto surface = PathFindingSurface(almost_doughnut, multi_polygon_type_fp(), 5);
  BOOST_CHECK_EQUAL(surface.in_surface({1,1}), boost::make_optional(size_t(0)));
  BOOST_CHECK_EQUAL(surface.in_surface({6,6}), boost::make_optional(size_t(0)));
  BOOST_CHECK_EQUAL(surface.in_surface({-10,-10}), boost::none);
  BOOST_CHECK_EQUAL(surface.in_surface({50,1}), boost::make_optional(size_t(0)));
  BOOST_CHECK_EQUAL(surface.in_surface({50,50}), boost::none);
  BOOST_CHECK_EQUAL(surface.in_surface({50,90}), boost::make_optional(size_t(0)));
}

BOOST_AUTO_TEST_SUITE_END() // nested_multipolygon_type_fp

constexpr double infinity = std::numeric_limits<double>::infinity();

BOOST_AUTO_TEST_CASE(open_space) {
  auto surface = PathFindingSurface(boost::none, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(1,1),
                               infinity, boost::none);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_CASE(simple) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = PathFindingSurface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(1,1),
                               infinity, boost::none);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_CASE(simple_limit0) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = PathFindingSurface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(1,1),
                               infinity, boost::make_optional(size_t(0)));
  BOOST_CHECK_EQUAL(ret, boost::none);
}

BOOST_AUTO_TEST_CASE(simple_limit1) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = PathFindingSurface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(1,1),
                               infinity, boost::make_optional(size_t(1)));
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_CASE(simple_limit_length200) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = PathFindingSurface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(100,100),
                               200, boost::none);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(100, 100));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_CASE(simple_limit_length100) {
  box_type_fp bounding_box = bg::return_envelope<box_type_fp>(point_type_fp(-100, -100));
  bg::expand(bounding_box, point_type_fp(100, 100));
  multi_polygon_type_fp keep_in;
  bg::convert(bounding_box, keep_in);
  auto surface = PathFindingSurface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(100,100),
                               100, boost::none);
  BOOST_CHECK_EQUAL(ret, boost::none);
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
  auto surface = PathFindingSurface(boost::none, keep_out, 0.1);
  auto ret = surface.find_path(point_type_fp(0, 0), point_type_fp(1, 1),
                               infinity, boost::none);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(1, 1));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
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
  auto surface = PathFindingSurface(boost::none, keep_out, 0.1);
  auto ret = surface.find_path(point_type_fp(0, 0), point_type_fp(50, 50),
                               infinity, boost::none);
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
  auto surface = PathFindingSurface(keep_in, keep_out, 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(10,10),
                               infinity, boost::none);

  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(3, 7));
  expected.push_back(point_type_fp(10, 10));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
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
  auto surface = PathFindingSurface(boost::none, keep_out, 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(10,10),
                               infinity, boost::none);

  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(3, 7));
  expected.push_back(point_type_fp(10, 10));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
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
  auto surface = PathFindingSurface(keep_in, keep_out, 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(5,5),
                               infinity, boost::none);

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
  auto surface = PathFindingSurface(keep_in, keep_out, 0.1);
  auto ret = surface.find_path(point_type_fp(0,0), point_type_fp(5,5),
                               infinity, boost::none);
  BOOST_CHECK_EQUAL(ret, boost::none);

  ret = surface.find_path(point_type_fp(0,0), point_type_fp(10,10),
                          infinity, boost::none);
  linestring_type_fp expected;
  expected.push_back(point_type_fp(0, 0));
  expected.push_back(point_type_fp(3, 7));
  expected.push_back(point_type_fp(10, 10));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
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
  auto surface = PathFindingSurface(keep_in, multi_polygon_type_fp(), 0.1);
  auto ret = surface.find_path(point_type_fp(1,9), point_type_fp(9,9),
                               infinity, boost::none);

  linestring_type_fp expected;
  expected.push_back(point_type_fp(1, 9));
  expected.push_back(point_type_fp(3, 3));
  expected.push_back(point_type_fp(7, 3));
  expected.push_back(point_type_fp(9, 9));
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_CASE(doughnut) {
  multi_polygon_type_fp almost_doughnut{
    {{{0,0}, {0,100}, {49,100}, {49,80},
      {30,70}, {20,20}, {80,20}, {80,80},
      {51,80}, {51,100}, {100,100},
      {100,0}, {0,0}}}};
  auto surface = PathFindingSurface(almost_doughnut, multi_polygon_type_fp(), 3);
  auto ret = surface.find_path(point_type_fp(10,10), point_type_fp(90,90),
                               infinity, boost::none);

  linestring_type_fp expected{{10, 10},{30, 70},{51, 80},{90, 90}};
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_CASE(barbell_search) {
  multi_polygon_type_fp barbell{{{{0,0}, {0,50}, {40,50}, {40,2}, {60,2},
                                  {60,50}, {100,50}, {100,0}, {0,0}}}};
  auto surface = PathFindingSurface(boost::none, barbell, 5);
  BOOST_CHECK_EQUAL(surface.find_path({-10,-10},{110,60},
                                      infinity, boost::none),
                    linestring_type_fp({{-10,-10},{40,2},{60,50},{110,60}}));
}

BOOST_AUTO_TEST_CASE(barbell_search_limit) {
  multi_polygon_type_fp barbell{{{{0,0}, {0,50}, {40,50}, {40,2}, {60,2},
                                  {60,50}, {100,50}, {100,0}, {0,0}}}};
  auto surface = PathFindingSurface(boost::none, barbell, 5);
  BOOST_CHECK_EQUAL(surface.find_path({-10,-10},{110,60},infinity, boost::make_optional(size_t(2))),
                    boost::none);
}

BOOST_AUTO_TEST_CASE(u_shape_keep_out) {
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
  auto surface = PathFindingSurface(boost::none, multi_polygon_type_fp{poly}, 0.1);
  auto ret = surface.find_path({5,5}, point_type_fp(-1,-1),
                               infinity, boost::none);
  linestring_type_fp expected{{5,5},{3,10},{0,10},{-1,-1}};
  BOOST_CHECK_EQUAL(ret, boost::make_optional(expected));
}

BOOST_AUTO_TEST_SUITE_END()
