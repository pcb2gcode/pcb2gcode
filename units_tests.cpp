#define BOOST_TEST_MODULE units tests
#include <boost/test/included/unit_test.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "units.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(units_tests);

BOOST_AUTO_TEST_CASE(parse_length_success) {
  po::value<Length>("foo");
}

BOOST_AUTO_TEST_SUITE_END()
