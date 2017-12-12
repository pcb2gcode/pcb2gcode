#define BOOST_TEST_DYN_LINK // to use shared libs
#define BOOST_TEST_MODULE eulerian_paths_test
#include <boost/test/unit_test.hpp>

#include "eulerian_paths.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(eulerian_paths);

struct PointLessThan {
  bool operator()(const point_type& a, const point_type& b) const {
    return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
  }
};

BOOST_AUTO_TEST_CASE(do_nothing_points) {
  BOOST_CHECK(0==0);
  get_eulerian_paths<point_type, PointLessThan>({{{1,1},{2,2},{3,3}}});
}

// 3x3 grid connected like a window pane:
// 1---2---3
// |   |   |
// 4---5---6
// |   |   |
// 7---8---9
BOOST_AUTO_TEST_CASE(window_pane) {
  BOOST_CHECK(0==0);
  vector<vector<int>> euler_paths = get_eulerian_paths<int>({
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
          });
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  BOOST_CHECK(edges_visited == 12);
  BOOST_CHECK(euler_paths.size() == 2);
}

// 3x3 grid connected like a window pane, but corners are longer paths:
// 1---2---3
// |   |   |
// 4---5---6
// |   |   |
// 7---8---9
BOOST_AUTO_TEST_CASE(window_pane_with_longer_corners) {
  BOOST_CHECK(0==0);
  vector<vector<int>> euler_paths = get_eulerian_paths<int>({
      {4,5},
      {5,6},
      {4,7,8},
      {2,5},
      {5,8},
      {6,9,8},
      {4,1,2},
      {2,3,6},
          });
  int edges_visited = 0;
  for (size_t i = 0; i < euler_paths.size(); i++) {
    edges_visited += euler_paths[i].size()-1;
    for (size_t j = 0; j < euler_paths[i].size(); j++) {
      printf("%d ", euler_paths[i][j]);
    }
    printf("\n");
  }
  BOOST_CHECK(edges_visited == 12);
  BOOST_CHECK(euler_paths.size() == 2);
}

BOOST_AUTO_TEST_SUITE_END()
