#define BOOST_TEST_MODULE tsp_solver_tests
#include <boost/test/unit_test.hpp>

#include "tsp_solver.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(tsp_solver_tests)

double get_distance(const point_type_fp& a, const point_type_fp& b) {
  return bg::distance(a, b);
}

double get_distance(const point_type_fp& a, const linestring_type_fp& b) {
  return bg::distance(a, b.front());
}

double get_distance(const linestring_type_fp& a, const linestring_type_fp& b) {
  return bg::distance(a.back(), b.front());
}

template <typename point_t, typename T>
double get_path_length(const vector<T>& path, const point_t& start) {
  if (path.size() == 0) {
    return 0;
  }
  double result = 0;
  result += get_distance(start, path[0]);
  for (size_t i = 1; i < path.size(); i++) {
    result += get_distance(path[i-1], path[i]);
  }
  return result;
}

template <typename T>
double get_path_length(const vector<T>& path) {
  if (path.size() == 0) {
    return 0;
  }
  return get_path_length(path, path[0]);
}

template <typename T>
void print_path(const vector<T>& path) {
  for (size_t i = 0; i < path.size(); i++) {
    cout << path[i] << "," << endl;
  }
}

template <>
void print_path(const vector<point_type_fp>& path) {
  for (size_t i = 0; i < path.size(); i++) {
    cout << "(" << path[i].x() << "," << path[i].y() << ") ";
  }
  cout << endl;
}

BOOST_AUTO_TEST_CASE(empty) {
  vector<point_type_fp> path;
  point_type_fp start(0,0);
  tsp_solver::nearest_neighbour(path, start);
  double nn = get_path_length(path, start);
  BOOST_CHECK_EQUAL(nn, 0);
  tsp_solver::tsp_2opt(path, start);
  double tsp_2opt = get_path_length(path, start);
  BOOST_CHECK_EQUAL(tsp_2opt, 0);
}

BOOST_AUTO_TEST_CASE(grid_10_by_10) {
  vector<point_type_fp> path;
  for (auto i = 0; i < 10; i++) {
    for (auto j = 0; j < 10; j++) {
      path.push_back(point_type_fp(i, j));
    }
  }
  point_type_fp start(0,0);
  tsp_solver::nearest_neighbour(path, start);
  double nn = get_path_length(path, start);
  tsp_solver::tsp_2opt(path, start);
  double tsp_2opt = get_path_length(path, start);
  BOOST_CHECK_LT(tsp_2opt, nn);
}

BOOST_AUTO_TEST_CASE(grid_10_by_10_no_start) {
  vector<point_type_fp> path;
  for (auto i = 0; i < 10; i++) {
    for (auto j = 0; j < 10; j++) {
      path.push_back(point_type_fp(i, j));
    }
  }
  point_type_fp start(-1,-1);
  tsp_solver::tsp_2opt(path, start);
  double tsp_2opt_with_start = get_path_length(path, start);
  tsp_solver::tsp_2opt<point_type_fp>(path);
  double tsp_2opt_without_start = get_path_length(path);
  BOOST_CHECK_LT(tsp_2opt_without_start, tsp_2opt_with_start);
}

BOOST_AUTO_TEST_CASE(reversable_paths) {
  vector<linestring_type_fp> path;
  for (auto i = 0; i < 10; i++) {
    path.push_back(linestring_type_fp{{static_cast<double>(i), 0},
                                      {static_cast<double>(i), 100}});
  }
  point_type_fp start(0,0);
  tsp_solver::tsp_2opt(path, start);
  double nn = get_path_length(path, start);
  BOOST_CHECK_LT(nn, 10);
}

BOOST_AUTO_TEST_SUITE_END()
