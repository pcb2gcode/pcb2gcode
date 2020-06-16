#define BOOST_TEST_MODULE trim_paths tests
#include <boost/test/unit_test.hpp>

#include <vector>

#include "geometry.hpp"
#include "bg_helpers.hpp"

#include "trim_paths.hpp"

using namespace std;

long double length(vector<pair<linestring_type_fp, bool>> lss) {
  long double total = 0;
  for (const auto& ls : lss) {
    total += bg::length(ls.first);
  }
  return total;
}

BOOST_AUTO_TEST_SUITE(trim_paths_tests)

BOOST_AUTO_TEST_CASE(empty) {
  vector<pair<linestring_type_fp, bool>> paths{};
  vector<pair<linestring_type_fp, bool>> backtracks{};
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{};
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(empty_path) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{};
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{};
  BOOST_CHECK_EQUAL(paths, paths);
}

BOOST_AUTO_TEST_CASE(trim_start) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{3,4}, {5,6}, {7,8}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_end) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{3,4}, {5,6}}, true},
    {{{5,6}, {7,8}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{1,2}, {3,4}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_both) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, true},
    {{{5,6}, {7,8}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{3,4}, {5,6}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_repeated) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, true},
    {{{1,2}, {3,4}}, true},
    {{{1,2}, {3,4}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{3,4}, {5,6}, {7,8}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(do_not_trim_non_repeated) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, true},
    {{{1,2}, {3,4}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_prefer_directed) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, false},
    {{{1,2}, {3,4}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_loop) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}, {1,2}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, true},
    {{{3,4}, {5,6}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{5,6}, {7,8}, {1,2}, {3,4}, {1,2}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_two_paths) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{1,2}, {3,4}}, true},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(trim_reversible) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}, {1,2}}, true},
    {{{1,2}, {3,4}, {1,2}, {3,4}, {5,6}, {7,8}, {1,2}}, true},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{5,6}, {3,4}}, false},
    {{{5,6}, {3,4}}, false},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{5,6}, {7,8}, {1,2}, {3,4}, {1,2}, {3,4}}, true},
    {{{5,6}, {7,8}, {1,2}, {3,4}, {1,2}, {3,4}}, true},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_CASE(directed_square_and_diagonal) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{0,0}, {0,5}}, false},
    {{{0,5}, {5,5}}, false},
    {{{5,5}, {5,0}}, false},
    {{{5,0}, {0,0}}, false},
    {{{5,5}, {0,0}}, false},
    {{{0,0}, {0,5}}, false},
    {{{0,5}, {5,5}}, false},
  };
  vector<pair<linestring_type_fp, bool>> backtracks{
    {{{0,0}, {0,5}}, false},
    {{{0,5}, {5,5}}, false},
  };
  trim_paths::trim_paths(paths, backtracks);
  vector<pair<linestring_type_fp, bool>> expected{
    {{{5,5}, {5,0}}, false},
    {{{5,0}, {0,0}}, false},
    {{{5,5}, {0,0}}, false},
    {{{0,0}, {0,5}}, false},
    {{{0,5}, {5,5}}, false},
  };
  BOOST_CHECK_EQUAL(paths, expected);
}

BOOST_AUTO_TEST_SUITE_END()
