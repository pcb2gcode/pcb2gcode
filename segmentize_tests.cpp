#define BOOST_TEST_MODULE segmentize tests
#include <boost/test/included/unit_test.hpp>

#include "segmentize.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(segmentize_tests);

struct PointLessThan {
  bool operator()(const point_type& a, const point_type& b) const {
    return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
  }
};

void print_result(const vector<segment_type_p>& result) {
  for (const auto& s : result) {
    printf("%ld %ld %ld %ld\n", s.low().x(), s.low().y(), s.high().x(), s.high().y());
  }
}


BOOST_AUTO_TEST_CASE(abuts) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(0, 0), point_type_p(2, 2)));
  ms.push_back(segment_type_p(point_type_p(1, 1), point_type_p(2, 0)));
  const auto& result = segmentize::segmentize(ms);
  BOOST_CHECK(result.size() == 3);
}

BOOST_AUTO_TEST_CASE(x_shape) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(0, 10000), point_type_p(10000, 9000)));
  ms.push_back(segment_type_p(point_type_p(10000, 10000), point_type_p(0, 0)));
  const auto& result = segmentize::segmentize(ms);
  BOOST_CHECK(result.size() == 4);
}

BOOST_AUTO_TEST_CASE(plus_shape) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(1, 2), point_type_p(3, 2)));
  ms.push_back(segment_type_p(point_type_p(2, 1), point_type_p(2, 3)));
  const auto& result = segmentize::segmentize(ms);
  BOOST_CHECK(result.size() == 4);
}

BOOST_AUTO_TEST_CASE(touching_no_overlap) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(1, 20), point_type_p(40, 50)));
  ms.push_back(segment_type_p(point_type_p(40, 50), point_type_p(80, 90)));
  const auto& result = segmentize::segmentize(ms);
  BOOST_CHECK(result.size() == 2);
}

BOOST_AUTO_TEST_CASE(parallel_with_overlap) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(0, 0)));
  ms.push_back(segment_type_p(point_type_p(9, 9), point_type_p(20, 20)));
  ms.push_back(segment_type_p(point_type_p(9, 9), point_type_p(30, 30)));
  const auto& result = segmentize::segmentize(ms);
  BOOST_CHECK(result.size() == 4);
  //print_result(result);
}

BOOST_AUTO_TEST_CASE(parallel_with_overlap_directed) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(0, 0)));
  ms.push_back(segment_type_p(point_type_p(9, 9), point_type_p(20, 20)));
  ms.push_back(segment_type_p(point_type_p(9, 9), point_type_p(30, 30)));
  const auto& result = segmentize::segmentize(ms, false);
  BOOST_CHECK(result.size() == 5);
  //print_result(result);
}

BOOST_AUTO_TEST_CASE(sort_segments) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(13, -4)));
  ms.push_back(segment_type_p(point_type_p(13, -4), point_type_p(10, 10)));
  ms.push_back(segment_type_p(point_type_p(13, -4), point_type_p(10, 10)));
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(13, -4)));
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(13, -4)));
  const auto& result = segmentize::segmentize(ms);
  BOOST_CHECK(result.size() == 1);
  //print_result(result);
}

BOOST_AUTO_TEST_SUITE_END()
