#define BOOST_TEST_MODULE options tests
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "options.hpp"
#include "units.hpp"
#include "available_drills.hpp"

#include <regex>
#include <utility>

using namespace std;

BOOST_AUTO_TEST_SUITE(options_tests)

void parse(const std::string& args) {
  std::vector<std::string> words;
  boost::split(words, args, boost::is_any_of(" "), boost::token_compress_on);
  std::vector<const char*> argc(words.size());
  int argv = words.size();
  for (unsigned int i = 0; i < words.size(); i++) {
    argc[i] = words[i].c_str();
  }
  cerr << endl << "Parsing: " << args << endl;
  options::get_vm().clear();
  options::parse(argv, &argc[0]);
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

// The code and message of the error from parsing and then checking args, or
// ERR_OK.
std::pair<ErrorCodes, std::string> get_check_error(const std::string& args) {
  try {
    parse(args);
    options::check_parameters();
  } catch (const pcb2gcode_parse_exception& e) {
    cerr << e.what() << endl;
    return {e.code(), e.what()};
  }
  return {ERR_OK, ""};
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
  BOOST_CHECK_EQUAL(get_count("pcb2gcode --offset 5mm", "offset"), 1);
  BOOST_CHECK_EQUAL(get_value<Length>("pcb2gcode --offset 5mm", "offset"), Length(0));
  BOOST_CHECK_EQUAL(get_value<std::vector<CommaSeparated<Length>>>("pcb2gcode --offset 5mm", "mill-diameters"),
                    std::vector<CommaSeparated<Length>>{{parse_unit<Length>("10mm")}});
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

BOOST_AUTO_TEST_CASE(mcp_option) {
  BOOST_CHECK_EQUAL(get_count("pcb2gcode --mcp", "mcp"), 1);
}

BOOST_AUTO_TEST_CASE(unknown_options_reported_together) {
  auto error = get_check_error("pcb2gcode --safety=3 --drill-diameters=0.8");
  BOOST_CHECK_EQUAL(error.first, ERR_UNKNOWNPARAMETER);
  BOOST_CHECK_NE(error.second.find("--safety=3"), std::string::npos);
  BOOST_CHECK_NE(error.second.find("--drill-diameters=0.8"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(missing_options_reported_together) {
  auto error = get_check_error("pcb2gcode --noconfigfile --back=board.gbr --drill=board.drl "
                               "--mill-feed=100 --mill-speed=15000 --drill-feed=50 "
                               "--drill-speed=10000");
  // The first problem decides the exit code.
  BOOST_CHECK_EQUAL(error.first, ERR_NOZSAFE);
  for (const std::string option : {"zsafe", "zchange", "zwork", "zdrill"}) {
    const std::string missing = "Missing --" + option + ",";
    BOOST_CHECK_NE(error.second.find(missing), std::string::npos);
    BOOST_CHECK_EQUAL(error.second.find(missing), error.second.rfind(missing));
  }
}

BOOST_AUTO_TEST_CASE(examples_in_errors_fix_them) {
  const std::string args = "pcb2gcode --noconfigfile --back=board.gbr --outline=outline.gbr "
                           "--drill=board.drl --al-back";
  auto error = get_check_error(args);
  BOOST_REQUIRE_NE(error.first, ERR_OK);
  std::string fixed = args;
  const std::regex example("For example: (--\\S+)");
  for (std::sregex_iterator it(error.second.begin(), error.second.end(), example), end;
       it != end; ++it) {
    fixed += " " + (*it)[1].str();
  }
  BOOST_CHECK_EQUAL(get_check_error(fixed).first, ERR_OK);
}

BOOST_AUTO_TEST_CASE(ignore_warnings_with_missing_options) {
  BOOST_CHECK_EQUAL(get_check_error("pcb2gcode --noconfigfile --ignore-warnings "
                                    "--back=board.gbr --outline=outline.gbr "
                                    "--drill=board.drl").first,
                    ERR_OK);
}

BOOST_AUTO_TEST_CASE(zchange_below_zdrill) {
  auto error = get_check_error("pcb2gcode --noconfigfile --drill=board.drl --zsafe=5mm "
                               "--zchange=-3mm --zdrill=-2.5mm --drill-feed=500mm/min "
                               "--drill-speed=10000rpm");
  BOOST_CHECK_EQUAL(error.first, ERR_ZSAFELOWERZCHANGE);
  BOOST_CHECK_NE(error.second.find("--zchange-absolute"), std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
