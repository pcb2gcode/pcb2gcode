#define BOOST_TEST_MODULE disjoint set tests
#include <boost/test/unit_test.hpp>

#include "disjoint_set.hpp"

BOOST_AUTO_TEST_SUITE(disjoint_set_tests)

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

BOOST_AUTO_TEST_CASE(simple2) {
  DisjointSet<int> d;
  BOOST_CHECK(d.find(3) != d.find(4));
  d.join(3,4);
  BOOST_CHECK(d.find(3) == d.find(4));
  BOOST_CHECK(d.find(1) != d.find(3));
  d.join(1,3);
  BOOST_CHECK(d.find(1) == d.find(3));
}

BOOST_AUTO_TEST_SUITE_END()
