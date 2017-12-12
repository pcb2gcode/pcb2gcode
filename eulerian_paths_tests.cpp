#define BOOST_TEST_DYN_LINK // to use shared libs
#define BOOST_TEST_MODULE eulerian_paths_test
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(eulerian_paths);

BOOST_AUTO_TEST_CASE(is0) {
  BOOST_CHECK(0==0);
  BOOST_CHECK(4==4);
}

BOOST_AUTO_TEST_SUITE_END()
