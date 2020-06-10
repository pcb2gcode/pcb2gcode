#define BOOST_TEST_MODULE strongly_connected_components tests
#include <boost/test/included/unit_test.hpp>

#include <vector>

#include "geometry.hpp"

#include "strongly_connected_components.hpp"

using namespace std;

namespace std {

static inline bool operator==(const point_type_fp& x, const point_type_fp& y) {
  return std::tie(x.x(), x.y()) == std::tie(y.x(), y.y());
}

template <typename T>
static inline bool operator==(
    const vector<T>& lhs,
    const vector<T>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (size_t i = 0; i < lhs.size(); i++) {
    if (!(lhs[i] == rhs[i])) {
      return false;
    }
  }
  return true;
}

static inline std::ostream& operator<<(std::ostream& out, const point_type_fp& p) {
  out << "{" << p.x() << "," << p.y() << "}";
  return out;
}

template <typename T>
static inline std::ostream& operator<<(std::ostream& out, const vector<T>& ls) {
  out << "{\n";
  for (const auto& l : ls) {
    std::stringstream ss;
    ss << l;
    char c;
    out << "  ";
    while (ss.get(c)) {
      out << c;
      if (c == '\n') {
        out << "  ";
      }
    }
    out << ",\n";
  }
  out << "}";
  return out;
}

} // namespace std

BOOST_AUTO_TEST_SUITE(strongly_connected_components_tests)

BOOST_AUTO_TEST_CASE(empty) {
  vector<pair<linestring_type_fp, bool>> paths{};
  const auto actual = strongly_connected_components::strongly_connected_components(paths);
  vector<vector<point_type_fp>> expected{};
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_CASE(wikipedia_example) {
  vector<pair<linestring_type_fp, bool>> paths{
    {{{0,0}, {1,1}}, false},
    {{{0,1}, {0,0}}, false},
    {{{1,0}, {0,0}}, false},
    {{{1,0}, {1,1}}, false},
    {{{1,0}, {2,0}}, false},
    {{{1,1}, {0,1}}, false},
    {{{2,0}, {1,0}}, false},
    {{{2,0}, {2,1}}, false},
    {{{2,1}, {1,1}}, false},
    {{{2,1}, {3,1}}, false},
    {{{3,0}, {2,0}}, false},
    {{{3,0}, {3,0}}, false},
    {{{3,0}, {3,1}}, false},
    {{{3,1}, {2,1}}, false},
  };
  const auto actual = strongly_connected_components::strongly_connected_components(paths);
  vector<vector<point_type_fp>> expected{
    {
      {0,1},
      {1,1},
      {0,0},
    },
    {
      {3,1},
      {2,1},
    },
    {
      {2,0},
      {1,0},
    },
    {
      {3,0},
    },
  };
  BOOST_CHECK_EQUAL(actual, expected);
}

BOOST_AUTO_TEST_SUITE_END()
