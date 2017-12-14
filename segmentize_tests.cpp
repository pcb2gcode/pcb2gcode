#define BOOST_TEST_DYN_LINK // to use shared libs
#define BOOST_TEST_MODULE segmentize_tests
#include <boost/test/unit_test.hpp>

#include "segmentize.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(segmentize_tests);

struct PointLessThan {
  bool operator()(const point_type& a, const point_type& b) const {
    return std::tie(a.x(), a.y()) < std::tie(b.x(), b.y());
  }
};

BOOST_AUTO_TEST_CASE(abuts) {
  vector<segment_type_fp_p> ms;
  ms.push_back(segment_type_fp_p(point_type_fp_p(0, 0), point_type_fp_p(2, 2)));
  ms.push_back(segment_type_fp_p(point_type_fp_p(1, 1), point_type_fp_p(2, 0)));
  const auto& result = segmentize(ms);
  BOOST_CHECK(result.size() == 3);
  //for (const auto& s : segmentize(ms)) {
  //  printf("%f %f %f %f\n", s.low().x(), s.low().y(), s.high().x(), s.high().y());
  //}
}

BOOST_AUTO_TEST_CASE(x_shape) {
  vector<segment_type_fp_p> ms;
  ms.push_back(segment_type_fp_p(point_type_fp_p(1.5, -1.5), point_type_fp_p(10, 10)));
  ms.push_back(segment_type_fp_p(point_type_fp_p(10, 0), point_type_fp_p(0, 10)));
  const auto& result = segmentize(ms);
  BOOST_CHECK(result.size() == 4);
  printf("%d\n", boost::polygon::intersects(ms[0], ms[1]));
  for (const auto& s : result) {
    printf("%f %f %f %f\n", s.low().x(), s.low().y(), s.high().x(), s.high().y());
  }
}

BOOST_AUTO_TEST_SUITE_END()
