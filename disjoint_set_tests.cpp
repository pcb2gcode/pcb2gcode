#define BOOST_TEST_MODULE disjoint set tests
#include <boost/test/unit_test.hpp>

#include "disjoint_set.hpp"

BOOST_AUTO_TEST_SUITE(path_finding_tests)

BOOST_AUTO_TEST_CASE(simple) {
  DisjointSet<int> d;
  BOOST_CHECK(d.find(2) != d.find(3));
  d.join(3,2);
  BOOST_CHECK(d.find(2) == d.find(3));
  BOOST_CHECK(d.find(9) != d.find(3));
  d.join(4,5);
  BOOST_CHECK(d.find(4) == d.find(5));
  BOOST_CHECK(d.find(4) != d.find(3));
  BOOST_CHECK(d.find(2) == d.find(3));
}

BOOST_AUTO_TEST_SUITE_END()
