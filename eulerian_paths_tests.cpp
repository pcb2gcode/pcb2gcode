#define BOOST_TEST_MODULE eulerian paths tests
#include <boost/test/included/unit_test.hpp>

#include <tuple>
#include "geometry_int.hpp"

namespace eulerian_paths {

static inline bool operator ==(const point_type& x, const point_type& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

} // namespace eulerian_paths

#include "eulerian_paths.hpp"
#include "geometry_int.hpp"

using std::vector;
using namespace eulerian_paths;

BOOST_AUTO_TEST_SUITE(eulerian_paths_tests)

struct PointLessThan {
  bool operator()(const point_type& a, const point_type& b) const {
    return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
  }
};

BOOST_AUTO_TEST_CASE(do_nothing_points) {
  linestring_type ls;
  ls.push_back(point_type(1,1));
  ls.push_back(point_type(2,2));
  ls.push_back(point_type(3,4));
  multi_linestring_type mls;
  mls.push_back(ls);
  multi_linestring_type result =
      get_eulerian_paths<point_type, linestring_type, multi_linestring_type, PointLessThan>(mls, vector<bool>(mls.size(), true));
  BOOST_CHECK_EQUAL(result.size(), 1UL);
}

// 3x3 grid connected like a window pane:
// 1---2---3
// |   |   |
// 4---5---6
// |   |   |
// 7---8---9
BOOST_AUTO_TEST_CASE(window_pane) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {1,2},
      {2,3},
      {4,5},
      {5,6},
      {7,8},
      {8,9},
      {1,4},
      {4,7},
      {2,5},
      {5,8},
      {3,6},
      {6,9},
    }, vector<bool>(12, true));
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 12);
  BOOST_CHECK_EQUAL(euler_paths.size(), 2UL);
}

// 3x3 grid connected like a window pane, but corners are longer paths:
// 1---2---3
// |   |   |
// 4---5---6
// |   |   |
// 7---8---9
BOOST_AUTO_TEST_CASE(window_pane_with_longer_corners) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {4,5},
      {5,6},
      {4,7,8},
      {2,5},
      {5,8},
      {6,9,8},
      {4,1,2},
      {2,3,6},
    }, vector<bool>(8, true));
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 12);
  BOOST_CHECK_EQUAL(euler_paths.size(), 2UL);
}

// Bridge
// 5---2---1---6
// |   |   |   |
// 3---4   7---8
BOOST_AUTO_TEST_CASE(bridge) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {5,2},
      {2,1},
      {1,6},
      {3,4},
      {7,8},
      {5,3},
      {2,4},
      {1,7},
      {6,8},
    }, vector<bool>(9, true));
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 9);
  BOOST_CHECK_EQUAL(euler_paths.size(), 1UL);
}

// Disjoint Loops and two degenerate paths
// 5---2   1---6  0---9
// |   |   |   |
// 3---4   7---8
BOOST_AUTO_TEST_CASE(disjoint_loops) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {5,2},
      {1,6},
      {3,4},
      {7,8},
      {5,3},
      {2,4},
      {1,7},
      {6,8},
      {0,9},
      {},
      {12}
    }, vector<bool>(11, true));
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 9);
  BOOST_CHECK_EQUAL(euler_paths.size(), 3UL);
}

// bidi and directional together
// 1-->2
// |   |
// v   |
// 3---4
BOOST_AUTO_TEST_CASE(mixed1) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {1,2},
      {1,3},
      {2,4},
      {3,4},
    }, vector<bool>{false, false, true, true});
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 4);
  BOOST_CHECK_EQUAL(euler_paths.size(), 2UL);
}

// bidi and directional together
// 1<--2
// |   |
// v   |
// 3---4
BOOST_AUTO_TEST_CASE(mixed2) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {2,1},
      {1,3},
      {2,4},
      {3,4},
    }, vector<bool>{false, false, true, true});
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 4);
  BOOST_CHECK_EQUAL(euler_paths.size(), 1UL);
}

// 3x3 grid bidi
// 1---2---3
// |   |   |
// |   v   |
// 4-->5<--6
// |   ^   |
// |   |   |
// 7---8---9
BOOST_AUTO_TEST_CASE(mixed3) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {1,2},
      {2,3},
      {1,4},
      {2,5},
      {3,6},
      {4,5},
      {6,5},
      {4,7},
      {8,5},
      {6,9},
      {7,8},
      {8,9},
    }, vector<bool>{true, true, true, false, true, false, false, true, false, true, true, true});
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 12);
  BOOST_CHECK_EQUAL(euler_paths.size(), 4UL);
}

// At least one of the paths must be turned around.
BOOST_AUTO_TEST_CASE(start_second) {
  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>({
      {0,1},
      {0,2},
    }, vector<bool>{true, true});
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 2);
  BOOST_CHECK_EQUAL(euler_paths.size(), 1UL);
}

// Directional paths with a loop.
BOOST_AUTO_TEST_CASE(directional_loop) {
  vector<vector<int>> input{{0, 0}, // here's a loop
                            {1, 0}, // it should connect to here
  };

  vector<vector<int>> euler_paths = get_eulerian_paths<int, vector<int>, vector<vector<int>>>(input, vector<bool>(input.size(), false));
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  printf("\n");
  BOOST_CHECK_EQUAL(edges_visited, 2);
  BOOST_CHECK_EQUAL(euler_paths.size(), 1UL);
}

BOOST_AUTO_TEST_CASE(must_start_tests) {
  vector<std::tuple<size_t, size_t, size_t, bool>> tests{
    // Sum = 0
    std::make_tuple(0, 0, 0, false),

    // Sum = 1
    std::make_tuple(0, 0, 1, true),
    std::make_tuple(0, 1, 0, false),
    std::make_tuple(1, 0, 0, true),

    // Sum = 2
    std::make_tuple(0, 0, 2, false),
    std::make_tuple(0, 1, 1, false),
    std::make_tuple(0, 2, 0, false),
    std::make_tuple(1, 0, 1, false),
    std::make_tuple(1, 1, 0, false),
    std::make_tuple(2, 0, 0, true),

    // Sum = 3
    std::make_tuple(0, 0, 3, true),
    std::make_tuple(0, 1, 2, true),
    std::make_tuple(0, 2, 1, false),
    std::make_tuple(0, 3, 0, false),
    std::make_tuple(1, 0, 2, true),
    std::make_tuple(1, 1, 1, true),
    std::make_tuple(1, 2, 0, false),
    std::make_tuple(2, 0, 1, true),
    std::make_tuple(2, 1, 0, true),
    std::make_tuple(3, 0, 0, true),

    // Sum = 4
    std::make_tuple(0, 0, 4, false),
    std::make_tuple(0, 1, 3, false),
    std::make_tuple(0, 2, 2, false),
    std::make_tuple(0, 3, 1, false),
    std::make_tuple(0, 4, 0, false),
    std::make_tuple(1, 0, 3, false),
    std::make_tuple(1, 1, 2, false),
    std::make_tuple(1, 2, 1, false),
    std::make_tuple(1, 3, 0, false),
    std::make_tuple(2, 0, 2, false),
    std::make_tuple(2, 1, 1, false),
    std::make_tuple(2, 2, 0, false),
    std::make_tuple(3, 0, 1, true),
    std::make_tuple(3, 1, 0, true),
    std::make_tuple(4, 0, 0, true),
  };
  for (const auto& test : tests) {
    BOOST_TEST_CONTEXT("must_start_helper(" << std::get<0>(test) << ", " << std::get<1>(test) << ", " << std::get<2>(test) << ")") {
      BOOST_CHECK_EQUAL(must_start_helper(std::get<0>(test), std::get<1>(test), std::get<2>(test)), std::get<3>(test));
    }
  }
}

BOOST_AUTO_TEST_SUITE_END()
