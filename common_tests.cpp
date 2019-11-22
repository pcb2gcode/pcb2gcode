#define BOOST_TEST_MODULE common tests
#include <boost/test/included/unit_test.hpp>

#include "common.hpp"

#include <boost/system/api_config.hpp>  // for BOOST_POSIX_API or BOOST_WINDOWS_API
#ifdef BOOST_WINDOWS_API
#define SEP "\\"
#endif

#ifdef BOOST_POSIX_API
#define SEP "/"
#endif

using std::vector;

BOOST_AUTO_TEST_SUITE(common_tests)

// Add more tests by comparing to python's os.path.join
BOOST_AUTO_TEST_CASE(build_filename_tests) {
  BOOST_CHECK_EQUAL(build_filename("", ""), "");
  BOOST_CHECK_EQUAL(build_filename("a", ""), "a" SEP);
  BOOST_CHECK_EQUAL(build_filename("", "b"), "b");
  BOOST_CHECK_EQUAL(build_filename("a", "b"), "a" SEP "b");
  BOOST_CHECK_EQUAL(build_filename("a" SEP, "b"), "a" SEP "b");
  BOOST_CHECK_EQUAL(build_filename("a" SEP, "b"), "a" SEP "b");
  BOOST_CHECK_EQUAL(build_filename("a" SEP SEP, "b"), "a" SEP SEP "b");
  BOOST_CHECK_EQUAL(build_filename(SEP, "b"), SEP "b");
  BOOST_CHECK_EQUAL(build_filename(SEP "a" SEP, "b"), SEP "a" SEP "b");
  BOOST_CHECK_EQUAL(build_filename(SEP "a" SEP SEP, "b"), SEP "a" SEP SEP "b");
  BOOST_CHECK_EQUAL(build_filename(SEP "a" SEP, SEP "b"), SEP "b");
  BOOST_CHECK_EQUAL(build_filename(SEP "a" SEP, SEP SEP "b"), SEP SEP "b");
  BOOST_CHECK_EQUAL(build_filename(SEP "a" SEP, ""), SEP "a" SEP);
  BOOST_CHECK_EQUAL(build_filename(SEP "a", ""), SEP "a" SEP);
}

BOOST_AUTO_TEST_SUITE_END()
