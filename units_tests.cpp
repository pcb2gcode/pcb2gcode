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

BOOST_AUTO_TEST_CASE(parse_length) {
  BOOST_CHECK_EQUAL(parse_unit<Length>("4").asInch(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Length>("25.4mm").asInch(200), 1);
  BOOST_CHECK_EQUAL(parse_unit<Length>("50.8mm").asInch(200), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>(" 50.8mm").asInch(200), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>(" 50.8mm    ").asInch(200), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>(" 50.8 mm ").asInch(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>("  \t50.8\tmm\t").asInch(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Length>("10000thou").asInch(0), 10);

  BOOST_CHECK_THROW(parse_unit<Length>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Length>("50.8seconds"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Length>("50.8s"), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_time) {
  BOOST_CHECK_EQUAL(parse_unit<Time>("4").asSecond(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Time>("4s").asSecond(1), 4);
  BOOST_CHECK_EQUAL(parse_unit<Time>("-10minutes").asSecond(2), -600);
  BOOST_CHECK_EQUAL(parse_unit<Time>(" 10 min").asSecond(2), 600);

  BOOST_CHECK_THROW(parse_unit<Time>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>("50.8inches"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>("50.8millimeters"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>("blahblah"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Time>(""), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_dimensionless) {
  BOOST_CHECK_EQUAL(parse_unit<Dimensionless>("4").as(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Dimensionless>("4").as(1), 4);
  BOOST_CHECK_EQUAL(parse_unit<Dimensionless>("4cycles").as(100), 4);

  BOOST_CHECK_THROW(parse_unit<Dimensionless>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Dimensionless>("50.8inches"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Dimensionless>("50.8millimeters"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Dimensionless>("blahblah"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Dimensionless>(""), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_frequency) {
  BOOST_CHECK_EQUAL(parse_unit<Frequency>("4").asPerMinute(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Frequency>("4").asPerMinute(1), 4);
  BOOST_CHECK_EQUAL(parse_unit<Frequency>("600   cycles\t/\tminute\t\t").asPerMinute(100), 600);
  BOOST_CHECK_EQUAL(parse_unit<Frequency>("2rotations/s").asPerMinute(100), 120);

  BOOST_CHECK_THROW(parse_unit<Frequency>("50.8mm/s"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Frequency>("50.8inches"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Frequency>("50.8millimeters"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Frequency>("blahblah"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Frequency>(""), po::validation_error);
}

BOOST_AUTO_TEST_CASE(parse_velocity) {
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("4").asInchPerMinute(2), 8);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("25.4mm/min").asInchPerMinute(100), 1);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("50.8mm/min").asInchPerMinute(100), 2);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>(" 50.8 mm/min ").asInchPerMinute(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>(" 50.8 mm per min ").asInchPerMinute(2), 2);
  BOOST_CHECK_EQUAL(parse_unit<Velocity>("  \t50.8\tmm\t/minutes").asInchPerMinute(2), 2);

  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8mm"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8 mm "), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8seconds"), po::validation_error);
  BOOST_CHECK_THROW(parse_unit<Velocity>("50.8s"), po::validation_error);
}

BOOST_AUTO_TEST_SUITE_END()
