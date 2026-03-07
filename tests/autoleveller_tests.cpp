#define BOOST_TEST_MODULE autoleveller tests
#include <boost/test/unit_test.hpp>

#include "geometry.hpp"
#include "bg_operators.hpp"

#include "autoleveller.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(autoleveller_tests)

BOOST_AUTO_TEST_CASE(ten_by_ten) {
  const auto actual = partition_segment(point_type_fp(0,0),
                                        point_type_fp(100,100),
                                        point_type_fp(0,0),
                                        point_type_fp(10, 10));
  const linestring_type_fp expected {
    {0, 0},
    {10, 10},
    {20, 20},
    {30, 30},
    {40, 40},
    {50, 50},
    {60, 60},
    {70, 70},
    {80, 80},
    {90, 90},
    {100, 100},
  };
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(horizontal_aligned) {
  const auto actual = partition_segment(point_type_fp(0,0),
                                        point_type_fp(0,100),
                                        point_type_fp(0,0),
                                        point_type_fp(10, 10));
  const linestring_type_fp expected {
    {0, 0},
    {0, 10},
    {0, 20},
    {0, 30},
    {0, 40},
    {0, 50},
    {0, 60},
    {0, 70},
    {0, 80},
    {0, 90},
    {0, 100},
  };
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(horizontal_unaligned) {
  const auto actual = partition_segment(point_type_fp(0.1,0.1),
                                        point_type_fp(0.1,19.9),
                                        point_type_fp(0,0),
                                        point_type_fp(10, 10));
  const linestring_type_fp expected {
    {0.1, 0.1},
    {0.1, 10},
    {0.1, 19.9},
  };
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(ten_by_ten_offset) {
  const auto actual = partition_segment(point_type_fp(0,0),
                                        point_type_fp(100,100),
                                        point_type_fp(1,1),
                                        point_type_fp(10, 10));
  const linestring_type_fp expected {
    {0, 0},
    {1, 1},
    {11, 11},
    {21, 21},
    {31, 31},
    {41, 41},
    {51, 51},
    {61, 61},
    {71, 71},
    {81, 81},
    {91, 91},
    {100, 100},
  };
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(source_equals_dest) {
  const auto actual = partition_segment(point_type_fp(0,0),
                                        point_type_fp(0,0),
                                        point_type_fp(1,2),
                                        point_type_fp(3, 4));
  const linestring_type_fp expected {
    {0, 0}
  };
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(skewed) {
  const auto actual = partition_segment(point_type_fp(5,5),
                                        point_type_fp(99,12),
                                        point_type_fp(0,0),
                                        point_type_fp(7, 5));
  const linestring_type_fp expected {
    {5, 5},
    {7, 5.14894},
    {14, 5.67021},
    {21, 6.19149},
    {28, 6.71277},
    {35, 7.23404},
    {42, 7.75532},
    {49, 8.2766},
    {56, 8.79787},
    {63, 9.31915},
    {70, 9.84043},
    {72.1429, 10},
    {77, 10.3617},
    {84, 10.883},
    {91, 11.4043},
    {98, 11.9255},
    {99, 12}
  };
  BOOST_CHECK_EQUAL(actual.size(), expected.size());
  for (size_t i = 0; i < actual.size(); i++) {
    BOOST_TEST_CONTEXT("actual[" << i << "] == expected[" << i << "]") {
      BOOST_CHECK_CLOSE(actual[i].x(), expected[i].x(), 0.001);
      BOOST_CHECK_CLOSE(actual[i].y(), expected[i].y(), 0.001);
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
