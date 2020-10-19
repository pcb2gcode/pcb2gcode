#define BOOST_TEST_MODULE outline_bridges tests
#include <boost/test/unit_test.hpp>

#include <vector>

#include "geometry.hpp"
#include "bg_operators.hpp"

#include "outline_bridges.hpp"

using namespace std;
using outline_bridges::makeBridges;

BOOST_AUTO_TEST_SUITE(outline_bridges_tests)

BOOST_AUTO_TEST_CASE(box) {
  linestring_type_fp path{{0,0}, {0,10}, {10,10}, {10,0}, {0,0}};
  auto ret = makeBridges(path, 4, 2);

  vector<size_t> expected_ret{1,4,7,10};
  linestring_type_fp expected_path{
    {0,0},
    {0,4},
    {0,6},
    {0,10},
    {4,10},
    {6,10},
    {10,10},
    {10,6},
    {10,4},
    {10,0},
    {6,0},
    {4,0},
    {0,0},
  };
  BOOST_CHECK_EQUAL(ret, expected_ret);
  BOOST_CHECK_EQUAL(path, expected_path);
}

BOOST_AUTO_TEST_CASE(rectangle) {
  linestring_type_fp path{{0,0}, {0,1}, {10,1}, {10,0}, {0,0}};
  auto ret = makeBridges(path, 2, 2);

  vector<size_t> expected_ret{2,6};
  linestring_type_fp expected_path{
    {0,0},
    {0,1},
    {4,1},
    {6,1},
    {10,1},
    {10,0},
    {6,0},
    {4,0},
    {0,0},
  };
  BOOST_CHECK_EQUAL(ret, expected_ret);
  BOOST_CHECK_EQUAL(path, expected_path);
}

BOOST_AUTO_TEST_SUITE_END()
