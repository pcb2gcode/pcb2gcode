#define BOOST_TEST_MODULE segmentize tests
#include <boost/test/included/unit_test.hpp>

#include "segmentize.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(segmentize_tests)

void print_result(const vector<std::pair<segment_type_p, bool>>& result) {
  for (const auto& segment_and_allow_reversal : result) {
    const auto& s = segment_and_allow_reversal.first;
    const auto& allow_reversal = segment_and_allow_reversal.second;
    printf("%ld %ld %ld %ld: %d\n", s.low().x(), s.low().y(), s.high().x(), s.high().y(), allow_reversal);
  }
}


BOOST_AUTO_TEST_CASE(abuts) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(0, 0), point_type_p(2, 2)));
  ms.push_back(segment_type_p(point_type_p(1, 1), point_type_p(2, 0)));
  const auto& result = segmentize::segmentize(ms, vector<bool>(ms.size(), true));
  BOOST_CHECK_EQUAL(result.size(), 3UL);
}

BOOST_AUTO_TEST_CASE(x_shape) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(0, 10000), point_type_p(10000, 9000)));
  ms.push_back(segment_type_p(point_type_p(10000, 10000), point_type_p(0, 0)));
  const auto& result = segmentize::segmentize(ms, vector<bool>(ms.size(), true));
  BOOST_CHECK_EQUAL(result.size(), 4UL);
}

BOOST_AUTO_TEST_CASE(plus_shape) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(1, 2), point_type_p(3, 2)));
  ms.push_back(segment_type_p(point_type_p(2, 1), point_type_p(2, 3)));
  const auto& result = segmentize::segmentize(ms, vector<bool>(ms.size(), true));
  BOOST_CHECK_EQUAL(result.size(), 4UL);
}

BOOST_AUTO_TEST_CASE(touching_no_overlap) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(1, 20), point_type_p(40, 50)));
  ms.push_back(segment_type_p(point_type_p(40, 50), point_type_p(80, 90)));
  const auto& result = segmentize::segmentize(ms, vector<bool>(ms.size(), true));
  BOOST_CHECK_EQUAL(result.size(), 2UL);
}

BOOST_AUTO_TEST_CASE(parallel_with_overlap) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(0, 0)));
  ms.push_back(segment_type_p(point_type_p(9, 9), point_type_p(20, 20)));
  ms.push_back(segment_type_p(point_type_p(30, 30), point_type_p(15, 15)));
  const auto& result = segmentize::segmentize(ms, {false, true, true});
  BOOST_CHECK_EQUAL(result.size(), 6UL);
  //print_result(result);
}

BOOST_AUTO_TEST_CASE(parallel_with_overlap_directed) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(0, 0)));
  ms.push_back(segment_type_p(point_type_p(9, 9), point_type_p(20, 20)));
  ms.push_back(segment_type_p(point_type_p(30, 30), point_type_p(15, 15)));
  const auto& result = segmentize::segmentize(ms, {true, false, false});
  BOOST_CHECK_EQUAL(result.size(), 7UL);
  //print_result(result);
}

BOOST_AUTO_TEST_CASE(sort_segments) {
  vector<segment_type_p> ms;
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(13, -4)));
  ms.push_back(segment_type_p(point_type_p(13, -4), point_type_p(10, 10)));
  ms.push_back(segment_type_p(point_type_p(13, -4), point_type_p(10, 10)));
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(13, -4)));
  ms.push_back(segment_type_p(point_type_p(10, 10), point_type_p(13, -4)));
  const auto& result = segmentize::segmentize(ms, vector<bool>(ms.size(), true));
  BOOST_CHECK_EQUAL(result.size(), 1UL);
  //print_result(result);
}

BOOST_AUTO_TEST_SUITE_END()
