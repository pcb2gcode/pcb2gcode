#define BOOST_TEST_MODULE options tests
#include <boost/test/included/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "options.hpp"

using namespace std;

BOOST_AUTO_TEST_SUITE(options_tests);

ErrorCodes get_error_code(const std::string& args) {
  std::vector<std::string> words;
  boost::split(words, args, boost::is_any_of(" "), boost::token_compress_on);
  const char* argc[words.size()];
  int argv = words.size();
  for (unsigned int i = 0; i < words.size(); i++) {
    argc[i] = words[i].c_str();
  }
  try {
    options::parse(argv, argc);
  } catch (pcb2gcode_parse_exception e) {
    return e.code();
  }
  return ERR_OK;
}

BOOST_AUTO_TEST_CASE(options) {
  BOOST_CHECK_EQUAL(get_error_code("pcb2gcode --foo"), 101);
}

BOOST_AUTO_TEST_SUITE_END()
