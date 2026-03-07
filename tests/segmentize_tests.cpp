#define BOOST_TEST_MODULE segmentize tests
#include <boost/test/unit_test.hpp>

#include <ostream>
#include "segmentize.hpp"
#include "bg_operators.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(segmentize_tests)

void print_result(const vector<std::pair<linestring_type_fp, bool>>& result) {
  cout << result;
}

BOOST_AUTO_TEST_CASE(abuts) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{0,0}, {2,2}}, true},
    {{{1,1}, {2,0}}, true},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 3UL);
}

BOOST_AUTO_TEST_CASE(x_shape) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{0,10000}, {10000,9000}}, true},
    {{{10000,10000}, {0,0}}, true},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 4UL);
}

BOOST_AUTO_TEST_CASE(plus_shape) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{1,2}, {3,2}}, true},
    {{{2,1}, {2,3}}, true},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 4UL);
}

BOOST_AUTO_TEST_CASE(touching_no_overlap) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{1,20}, {40,50}}, true},
    {{{40,50}, {80,90}}, true},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 2UL);
}

BOOST_AUTO_TEST_CASE(parallel_with_overlap) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{10,10}, {0,0}}, false},
    {{{9,9}, {20,20}}, true},
    {{{30,30}, {15,15}}, true},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 7UL);
  //print_result(result);
}

BOOST_AUTO_TEST_CASE(parallel_with_overlap_directed) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{10,10}, {0,0}}, true},
    {{{9,9}, {20,20}}, false},
    {{{30,30}, {15,15}}, false},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 7UL);
  //print_result(result);
}

BOOST_AUTO_TEST_CASE(sort_segments) {
  vector<pair<linestring_type_fp, bool>> ms = {
    {{{10,10}, {13,-4}}, true},
    {{{13,-4}, {10,10}}, true},
    {{{13,-4}, {10,10}}, true},
    {{{10, 10}, {13, -4}}, true},
    {{{10, 10}, {13, -4}}, true},
  };
  const auto& result = segmentize::segmentize_paths(ms);
  BOOST_CHECK_EQUAL(result.size(), 5UL);
  //print_result(result);
}

BOOST_AUTO_TEST_SUITE_END()
