#define BOOST_TEST_MODULE backtrack tests
#include <boost/test/unit_test.hpp>

#include <vector>

#include "geometry.hpp"
#include "bg_operators.hpp"
#include "bg_helpers.hpp"

#include "backtrack.hpp"

using namespace std;

long double length(vector<pair<linestring_type_fp, bool>> lss) {
  long double total = 0;
  for (const auto& ls : lss) {
    total += bg::length(ls.first);
  }
  return total;
}

BOOST_AUTO_TEST_SUITE(backtrack_tests)

BOOST_AUTO_TEST_CASE(empty) {
  vector<pair<linestring_type_fp, bool>> paths{};
  const auto actual = backtrack::backtrack(paths, 1,100,100,100, 100);
  vector<pair<linestring_type_fp, bool>> expected{};
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(square) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{0,0}, {0,1}}, true},
    {{{0,1}, {1,1}}, true},
    {{{1,1}, {1,0}}, true},
    {{{1,0}, {0,0}}, true},
  };
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  vector<pair<linestring_type_fp, bool>> expected{};
  BOOST_CHECK_EQUAL(actual, expected);
}

static vector<pair<linestring_type_fp, bool>> make_grid(point_type_fp p0, point_type_fp p1, int lines) {
  vector<pair<linestring_type_fp, bool>> ret;

  const int x_lines = lines;
  const int y_lines = lines;
  for (auto x = 0; x < x_lines; x++) {
    for (auto y = 0; y < y_lines; y++) {
      point_type_fp a = {p0.x() * (x_lines-1-x)/(x_lines-1) + p1.x() * x/(x_lines-1),
                         p0.y() * (y_lines-1-y)/(y_lines-1) + p1.y() * y/(y_lines-1)};
      if (x + 1 <= x_lines - 1) {
        point_type_fp b = {p0.x() * (x_lines-1-x-1)/(x_lines-1) + p1.x() * (x+1)/(x_lines-1),
                           p0.y() * (y_lines-1-y)/(y_lines-1) + p1.y() * y/(y_lines-1)};
        ret.push_back({{a,b}, true});
      }
      if (y + 1 <= y_lines - 1) {
        point_type_fp c = {p0.x() * (x_lines-1-x)/(x_lines-1) + p1.x() * (x)/(x_lines-1),
                           p0.y() * (y_lines-1-y-1)/(y_lines-1) + p1.y() * (y+1)/(y_lines-1)};
        ret.push_back({{a,c}, true});
      }
    }
  }
  return ret;
}

BOOST_AUTO_TEST_CASE(grid) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 4);
  BOOST_CHECK_EQUAL(actual.size(), 4);
}

BOOST_AUTO_TEST_CASE(wide_grid) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,20}, 3);
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 22);
  BOOST_CHECK_EQUAL(actual.size(), 4);
}

BOOST_AUTO_TEST_CASE(tall_grid) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {20,2}, 3);
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 22);
  BOOST_CHECK_EQUAL(actual.size(), 4);
}

BOOST_AUTO_TEST_CASE(two_grids) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  auto grid2 = make_grid({10,10}, {12,12}, 3);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 8);
  BOOST_CHECK_EQUAL(actual.size(), 8);
}

BOOST_AUTO_TEST_CASE(two_grids_connected_at_corner) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  auto grid2 = make_grid({10,0}, {12,2}, 3);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,0}, {10,0}}, true});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 18);
  BOOST_CHECK_EQUAL(actual.size(), 11);
}

BOOST_AUTO_TEST_CASE(two_grids_connected_at_corner_directed) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  auto grid2 = make_grid({10,0}, {12,2}, 3);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,0}, {10,0}}, false});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 18);
  BOOST_CHECK_EQUAL(actual.size(), 11);
}

BOOST_AUTO_TEST_CASE(two_grids_connected_at_side) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  auto grid2 = make_grid({10,0}, {12,2}, 3);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,1}, {10,1}}, true});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 16);
  BOOST_CHECK_EQUAL(actual.size(), 9);
}

BOOST_AUTO_TEST_CASE(two_grids_connected_at_side_directed) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  auto grid2 = make_grid({10,0}, {12,2}, 3);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,1}, {10,1}}, false});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 16);
  BOOST_CHECK_EQUAL(actual.size(), 9);
}

BOOST_AUTO_TEST_CASE(two_grids_connected_at_2_corners) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 3);
  auto grid2 = make_grid({10,0}, {12,2}, 3);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,0}, {10,0}}, true});
  paths.push_back({{{2,2}, {10,2}}, true});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 8);
  BOOST_CHECK_EQUAL(actual.size(), 8);
}

BOOST_AUTO_TEST_CASE(two_squares_connected_at_2_corners_directed) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 2);
  auto grid2 = make_grid({10,0}, {12,2}, 2);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,0}, {10,0}}, false});
  paths.push_back({{{2,2}, {10,2}}, false});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 4);
  BOOST_CHECK_EQUAL(actual.size(), 2);
}

BOOST_AUTO_TEST_CASE(two_squares_connected_at_2_corners_undirected) {
  vector<pair<linestring_type_fp, bool>> paths = make_grid({0,0}, {2,2}, 2);
  auto grid2 = make_grid({10,0}, {12,2}, 2);
  paths.insert(paths.cend(), grid2.cbegin(), grid2.cend());
  paths.push_back({{{2,0}, {10,0}}, true});
  paths.push_back({{{2,2}, {10,2}}, true});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 4);
  BOOST_CHECK_EQUAL(actual.size(), 2);
}

BOOST_AUTO_TEST_CASE(two_directed_lines) {
  vector<pair<linestring_type_fp, bool>> paths;
  paths.push_back({{{0,0}, {0,5}}, false});
  paths.push_back({{{0,0}, {5,0}}, false});
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 0);
  BOOST_CHECK_EQUAL(actual.size(), 0);
}

BOOST_AUTO_TEST_CASE(directed_square_and_diagonal) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{0,0}, {0,5}}, false},
    {{{0,5}, {5,5}}, false},
    {{{5,5}, {5,0}}, false},
    {{{5,0}, {0,0}}, false},
    {{{5,5}, {0,0}}, false},
  };
  const auto actual = backtrack::backtrack(paths, 1,100,1,100, 100);
  BOOST_TEST_MESSAGE("actual is " << actual);
  BOOST_CHECK_EQUAL(length(actual), 10);
  BOOST_CHECK_EQUAL(actual.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
