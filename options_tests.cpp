#define BOOST_TEST_MODULE options tests
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "options.hpp"
#include "units.hpp"
#include "available_drills.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(options_tests)

void parse(const std::string& args) {
  std::vector<std::string> words;
  boost::split(words, args, boost::is_any_of(" "), boost::token_compress_on);
  const char* argc[words.size()];
  int argv = words.size();
  for (unsigned int i = 0; i < words.size(); i++) {
    argc[i] = words[i].c_str();
  }
  cerr << endl << "Parsing: " << args << endl;
  options::get_vm().clear();
  options::parse(argv, argc);
}

ErrorCodes get_error_code(const std::string& args) {
  try {
    parse(args);
  } catch (const pcb2gcode_parse_exception& e) {
    cerr << e.what() << endl;
    return e.code();
  }
  return ERR_OK;
}

template<typename variable_t>
variable_t get_value(const std::string& args, const std::string& variable) {
  BOOST_CHECK_EQUAL(get_error_code(args), ERR_OK);
  BOOST_CHECK_NO_THROW(options::get_vm().at(variable).as<variable_t>());
  return options::get_vm().at(variable).as<variable_t>();
}

int get_count(const std::string& args, const std::string& variable) {
  BOOST_CHECK_EQUAL(get_error_code(args), ERR_OK);
  return options::get_vm().count(variable);
}

BOOST_AUTO_TEST_CASE(foo) {
  BOOST_CHECK_EQUAL(get_error_code("pcb2gcode --foo"), 101);
}

BOOST_AUTO_TEST_CASE(available_drills) {
  std::stringstream drills_text;
  auto drills = get_value<std::vector<AvailableDrills>>(
      "pcb2gcode --drills-available 5mm --drills-available 15mm",
      "drills-available");
  drills_text << drills;
  BOOST_CHECK_EQUAL(drills_text.str(), "0.005 m, 0.015 m");
}

BOOST_AUTO_TEST_CASE(offset) {
  BOOST_CHECK_EQUAL(get_count("pcb2gcode --offset 5mm", "offset"), 0);
  BOOST_CHECK_EQUAL(get_value<std::vector<CommaSeparated<Length>>>("pcb2gcode --offset 5mm", "mill-diameters"),
                    std::vector<CommaSeparated<Length>>{{parse_unit<Length>("10mm")}});
  BOOST_CHECK_THROW(parse("pcb2gcode --offset 5mm --mill-diameters 5mm"), pcb2gcode_parse_exception);
}

BOOST_AUTO_TEST_CASE(mill_diameters) {
  BOOST_CHECK_EQUAL(get_value<std::vector<CommaSeparated<Length>>>("pcb2gcode --mill-diameters 1mm,2mm 3mm --mill-diameters 4mm", "mill-diameters"),
                    (std::vector<CommaSeparated<Length>>{
                      {parse_unit<Length>("1mm"), parse_unit<Length>("2mm")},
                      {parse_unit<Length>("3mm")},
                      {parse_unit<Length>("4mm")}
                    }));
}

BOOST_AUTO_TEST_CASE(milling_overlap) {
  BOOST_CHECK_EQUAL(
      (get_value<boost::variant<Length, Percent>>("pcb2gcode", "milling-overlap")),
      (boost::variant<Length, Percent>(parse_unit<Percent>("50%"))));
  BOOST_CHECK_EQUAL(
      (get_value<boost::variant<Length, Percent>>("pcb2gcode --milling-overlap 10%", "milling-overlap")),
      (boost::variant<Length, Percent>(parse_unit<Percent>("10%"))));
  BOOST_CHECK_EQUAL(
      (get_value<boost::variant<Length, Percent>>("pcb2gcode --milling-overlap 1mm", "milling-overlap")),
      (boost::variant<Length, Percent>(parse_unit<Length>("1mm"))));
  BOOST_CHECK_THROW(
      (parse("pcb2gcode --milling-overlap 1rpm")),
      pcb2gcode_parse_exception);
}

BOOST_AUTO_TEST_SUITE_END()
