#define BOOST_TEST_MODULE units tests
#include <boost/test/included/unit_test.hpp>

#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "units.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(units_tests);

template <typename dimension_t>
dimension_t parse_unit(const std::string& s) {
  po::options_description desc("Allowed options");
  desc.add_options()
      ("test_option", po::value<dimension_t>(), "test option");
  po::variables_map vm;
  po::store(po::command_line_parser(vector<std::string>{"--test_option", s}).options(desc).run(), vm);
  return vm["test_option"].as<dimension_t>();
}

BOOST_AUTO_TEST_CASE(parse_length_success) {
  BOOST_CHECK_EQUAL(parse_unit<Length>("4").asInch(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Length>("25.4mm").asInch(2), 1);
}

BOOST_AUTO_TEST_SUITE_END()
