#define BOOST_TEST_MODULE tsp_solver_tests
#include <boost/test/included/unit_test.hpp>

#include "tsp_solver.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(tsp_solver_tests);

double get_distance(const icoordpair& a, const icoordpair& b) {
  return sqrt((a.first-b.first)*(a.first-b.first) +
              (a.second-b.second)*(a.second-b.second));
}

template <typename T>
double get_path_length(const vector<T>& path, T start) {
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
void print_path(const vector<icoordpair>& path) {
  for (size_t i = 0; i < path.size(); i++) {
    cout << "(" << path[i].first << "," << path[i].second << ") ";
  }
  cout << endl;
}

BOOST_AUTO_TEST_CASE(empty) {
  vector<icoordpair> path;
  icoordpair start(0,0);
  tsp_solver::nearest_neighbour(path, start);
  double nn = get_path_length(path, start);
  BOOST_CHECK_EQUAL(nn, 0);
  tsp_solver::tsp_2opt(path, start);
  double tsp_2opt = get_path_length(path, start);
  BOOST_CHECK_EQUAL(tsp_2opt, 0);
}

BOOST_AUTO_TEST_CASE(grid_10_by_10) {
  vector<icoordpair> path;
  for (auto i = 0; i < 10; i++) {
    for (auto j = 0; j < 10; j++) {
      path.push_back(icoordpair(i, j));
    }
  }
  icoordpair start(0,0);
  tsp_solver::nearest_neighbour(path, start);
  double nn = get_path_length(path, start);
  tsp_solver::tsp_2opt(path, start);
  double tsp_2opt = get_path_length(path, start);
  BOOST_CHECK_LT(tsp_2opt, nn);
}

BOOST_AUTO_TEST_CASE(grid_10_by_10_no_start) {
  vector<icoordpair> path;
  for (auto i = 0; i < 10; i++) {
    for (auto j = 0; j < 10; j++) {
      path.push_back(icoordpair(i, j));
    }
  }
  icoordpair start(-1,-1);
  tsp_solver::tsp_2opt(path, start);
  double tsp_2opt_with_start = get_path_length(path, start);
  tsp_solver::tsp_2opt<icoordpair>(path);
  double tsp_2opt_without_start = get_path_length(path);
  BOOST_CHECK_LT(tsp_2opt_without_start, tsp_2opt_with_start);
}

BOOST_AUTO_TEST_SUITE_END()
