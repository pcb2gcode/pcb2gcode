#define BOOST_TEST_DYN_LINK // to use shared libs
#define BOOST_TEST_MODULE eulerian_paths_test
#include <boost/test/unit_test.hpp>

#include "eulerian_paths.hpp"

BOOST_AUTO_TEST_SUITE(eulerian_paths);

BOOST_AUTO_TEST_CASE(do_nothing) {
  BOOST_CHECK(0==0);
  get_eulerian_paths<int>({{1,2,3}});
}

BOOST_AUTO_TEST_SUITE_END()
