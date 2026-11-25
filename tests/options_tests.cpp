#define BOOST_TEST_MODULE options tests
#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
namespace po = boost::program_options;

#include "options.hpp"
#include "option_metadata.hpp"
#include "units.hpp"
#include "available_drills.hpp"

#include <nlohmann/json.hpp>

#include <regex>
#include <set>
#include <utility>

using namespace std;
using json = nlohmann::json;

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

BOOST_AUTO_TEST_CASE(stray_arguments_reported) {
  auto error = get_check_error("pcb2gcode board.gbr --zsafe 5mm extra");
  BOOST_CHECK_EQUAL(error.first, ERR_UNKNOWNPARAMETER);
  BOOST_CHECK_NE(error.second.find("Unexpected argument board.gbr"), std::string::npos);
  BOOST_CHECK_NE(error.second.find("Unexpected argument extra"), std::string::npos);
  // 5mm is the value of --zsafe.
  BOOST_CHECK_EQUAL(error.second.find("5mm"), std::string::npos);
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
  BOOST_CHECK_EQUAL(get_check_error("pcb2gcode --noconfigfile --drill=board.drl --zsafe=5mm "
                                    "--zchange=-3mm --zdrill=-2.5mm --drill-feed=500mm/min "
                                    "--drill-speed=10000rpm --zchange-absolute").first,
                    ERR_OK);
}

BOOST_AUTO_TEST_CASE(range_rules) {
  const string start = "pcb2gcode --noconfigfile --zsafe=5mm --zchange=10mm";
  BOOST_CHECK_EQUAL(get_check_error(start + " --tile-x=0").first, ERR_NEGATIVETILEX);
  BOOST_CHECK_EQUAL(get_check_error(start + " --tile-x=1").first, ERR_OK);
  BOOST_CHECK_EQUAL(get_check_error(start + " --al-front --software=linuxcnc --al-x=0 "
                                    "--al-y=1mm --al-probefeed=100mm/min").first,
                    ERR_NEGATIVEALX);

  const string outline = start + " --outline=outline.gbr --zcut=-2mm --cutter-diameter=2mm "
                         "--cut-feed=100mm/min --cut-speed=10000rpm";
  BOOST_CHECK_EQUAL(get_check_error(outline + " --cut-infeed=0.0009in").first, ERR_LOWCUTINFEED);
  BOOST_CHECK_EQUAL(get_check_error(outline + " --cut-infeed=0.001in").first, ERR_OK);
  // Bare lengths are inches, or millimeters with --metric.
  BOOST_CHECK_EQUAL(get_check_error(outline + " --cut-infeed=0.0009").first, ERR_LOWCUTINFEED);
  BOOST_CHECK_EQUAL(get_check_error(outline + " --metric --cut-infeed=0.03").first, ERR_OK);

  const string milldrill = start + " --drill=board.drl --zdrill=-2mm --drill-feed=100mm/min "
                           "--drill-speed=10000rpm --min-milldrill-hole-diameter=0 --zcut=-2mm "
                           "--cutter-diameter=1mm --cut-feed=100mm/min --cut-speed=10000rpm "
                           "--cut-infeed=1mm";
  BOOST_CHECK_EQUAL(get_check_error(milldrill).first, ERR_OK);
  BOOST_CHECK_EQUAL(get_check_error(milldrill + " --milldrill-stepover=100%").first, ERR_OK);
  BOOST_CHECK_EQUAL(get_check_error(milldrill + " --milldrill-stepover=150%").first,
                    ERR_INVALIDMILLDRILLSTEPOVER);
  BOOST_CHECK_EQUAL(get_check_error(milldrill + " --milldrill-stepover=0").first,
                    ERR_INVALIDMILLDRILLSTEPOVER);
  BOOST_CHECK_EQUAL(get_check_error(milldrill + " --min-milldrill-entry-diameter=100%").first,
                    ERR_LOWMILLDRILLENTRY);
  BOOST_CHECK_EQUAL(get_check_error(milldrill + " --max-milldrill-entry-diameter=140%").first,
                    ERR_MILLDRILLENTRYORDER);
}

BOOST_AUTO_TEST_CASE(deprecated_sides) {
  const string drill = "pcb2gcode --noconfigfile --zsafe=5mm --zchange=10mm --drill=board.drl "
                       "--zdrill=-2mm --drill-feed=100mm/min --drill-speed=10000rpm";
  BOOST_CHECK_EQUAL(get_check_error(drill + " --drill-front").first, ERR_OK);
  BOOST_CHECK_EQUAL(get_check_error(drill + " --drill-front --drill-side=back").first,
                    ERR_BOTHDRILLFRONTSIDE);
  const string outline = "pcb2gcode --noconfigfile --zsafe=5mm --zchange=10mm "
                         "--outline=outline.gbr --zcut=-2mm --cutter-diameter=2mm "
                         "--cut-feed=100mm/min --cut-speed=10000rpm --cut-infeed=1mm";
  BOOST_CHECK_EQUAL(get_check_error(outline + " --cut-front=false").first, ERR_OK);
  BOOST_CHECK_EQUAL(get_check_error(outline + " --cut-front --cut-side=front").first,
                    ERR_BOTHCUTFRONTSIDE);
}

// What check_parameters() prints to stderr for args.
string check_warnings(const string& args) {
  std::stringstream warnings;
  struct RestoreCerr {
    std::streambuf* buffer;
    ~RestoreCerr() { cerr.rdbuf(buffer); }
  } restore{cerr.rdbuf(warnings.rdbuf())};
  BOOST_CHECK_EQUAL(get_check_error(args).first, ERR_OK);
  return warnings.str();
}

BOOST_AUTO_TEST_CASE(warnings) {
  const string start = "pcb2gcode --noconfigfile --zsafe=5mm --zchange=10mm";
  BOOST_CHECK_NE(check_warnings(start + " --tolerance=1").find("high tolerance"), string::npos);
  BOOST_CHECK_NE(check_warnings(start + " --tolerance=0").find("Deviation from commanded toolpath"),
                 string::npos);
  BOOST_CHECK_EQUAL(check_warnings(start + " --tolerance=0.001").find("Warning"), string::npos);
  BOOST_CHECK_NE(check_warnings(start + " --svg=board.svg").find("--svg is deprecated"),
                 string::npos);
  BOOST_CHECK_NE(check_warnings(start + " --back=board.gbr --zwork=0.1mm --mill-feed=1 "
                                "--mill-speed=1").find("--zwork) is greater than zero"),
                 string::npos);
  BOOST_CHECK_NE(check_warnings(start + " --drill=board.drl --zdrill=-2mm --drill-feed=1 "
                                "--drill-speed=1 --drill-front").find("drill-front is deprecated"),
                 string::npos);
  BOOST_CHECK_NE(check_warnings(start + " --outline=outline.gbr --zcut=-2mm "
                                "--cutter-diameter=2mm --cut-feed=1 --cut-speed=1 "
                                "--cut-infeed=1mm --cut-front").find("cut-front is deprecated"),
                 string::npos);
}

BOOST_AUTO_TEST_CASE(conditions) {
  parse("pcb2gcode --noconfigfile --drill=board.drl");
  const po::variables_map& vm = options::get_vm();
  const auto set = &OptionCondition::set;
  // An option without a value is never equal to anything.
  BOOST_CHECK(!OptionCondition::equal("software", "linuxcnc").holds(vm));
  BOOST_CHECK(OptionCondition::equal("drill-side", "auto").holds(vm));
  BOOST_CHECK(!OptionCondition::given("drill-side").holds(vm));
  BOOST_CHECK(OptionCondition::given("drill").holds(vm));
  // Only numbers can be compared with inf.
  BOOST_CHECK_THROW(OptionCondition::finite("drill").holds(vm), std::logic_error);
  BOOST_CHECK_THROW(OptionCondition::finite("mill-diameters").holds(vm), std::logic_error);
  BOOST_CHECK_THROW(OptionCondition::finite("pre-milling-gcode").holds(vm), std::logic_error);
  BOOST_CHECK(!(set("front") || set("back") || set("outline")).holds(vm));
  BOOST_CHECK((set("drill") && !set("front") && set("zchange-absolute")).holds(vm));
  // Chains stay flat.
  BOOST_CHECK_EQUAL((set("front") || set("back") || set("outline")).to_json().dump(),
                    R"({"any":[{"set":"front"},{"set":"back"},{"set":"outline"}]})");
  BOOST_CHECK_EQUAL((set("a") && (set("b") && set("c"))).to_json().dump(),
                    R"({"all":[{"set":"a"},{"set":"b"},{"set":"c"}]})");
}

// A catalog made by hand, for what pcb2gcode's own options don't use.
BOOST_AUTO_TEST_CASE(small_catalog) {
  po::options_description description;
  description.add_options()
      ("width", po::value<Length>(), "a width")
      ("depth", po::value<Length>()->default_value(parse_unit<Length>("1mm")), "a depth");
  const std::vector<OptionGroup> groups{{"test", "Test options", "always", false, {"width"}}};
  const std::vector<NamedCondition> conditions{{"always", "Always.", OptionCondition()}};
  const std::map<string, OptionDetails> details;
  const std::vector<OptionRule> rules{
      OptionRule::range("width").below("2mm").fails_with(ERR_INVALIDPARAMETER, "too wide"),
      OptionRule::greater_or_equal("width", "depth").fails_with(ERR_INVALIDPARAMETER, "shallow")};
  const OptionCatalog catalog{description, groups, conditions, details, rules};

  for (const auto& width_and_errors : std::vector<std::pair<string, std::vector<string>>>{
           {"1mm", {}}, {"2mm", {"too wide"}}, {"0.5mm", {"shallow"}}}) {
    po::variables_map vm;
    po::store(po::command_line_parser(std::vector<string>{"--width=" + width_and_errors.first})
                  .options(description).run(), vm);
    std::vector<string> errors;
    for (const auto& rule : rules) {
      const auto error = rule.check(vm, catalog, 1);
      if (error) {
        errors.push_back(error->what);
      }
    }
    BOOST_CHECK(errors == width_and_errors.second);
  }
  const json dumped = json::parse(dump_catalog(catalog));
  BOOST_CHECK_EQUAL(dumped["rules"][0]["max"], "2mm");
  BOOST_CHECK_EQUAL(dumped["rules"][0]["max_exclusive"], true);
  BOOST_CHECK_EQUAL(dumped["options"][0]["range"], (json{{"max", "2mm"}, {"max_exclusive", true}}));

  // Mistakes in the catalog are bugs, so they throw.
  po::variables_map vm;
  vm.insert({"height", po::variable_value(boost::any(Length(1)), false)});
  BOOST_CHECK_THROW(OptionRule::range("height").at_least("0").check(vm, catalog, 1),
                    std::logic_error);
  BOOST_CHECK_THROW(OptionRule::required("height", "a height", "1mm").when("never")
                        .check(vm, catalog, 1),
                    std::logic_error);
  const std::vector<OptionGroup> wrong_groups{{"test", "Test options", "always", false, {"height"}}};
  BOOST_CHECK_THROW(dump_catalog(OptionCatalog{description, wrong_groups, conditions, details,
                                               rules}),
                    std::logic_error);
}

BOOST_AUTO_TEST_CASE(rule_errors_name_their_options) {
  parse("pcb2gcode --noconfigfile --zsafe=1mm --zchange=10mm --back=board.gbr --zwork=2mm "
        "--mill-feed=1 --mill-speed=1");
  std::vector<OptionError> errors;
  check_rules(options::catalog(), options::get_vm(), errors);
  BOOST_REQUIRE_EQUAL(errors.size(), 1U);
  BOOST_CHECK_EQUAL(errors[0].code, ERR_ZSAFELOWERZWORK);
  BOOST_CHECK(errors[0].options == (std::vector<std::string>{"zsafe", "zwork"}));
}

json dump() {
  return json::parse(options::dump_options());
}

json find_option(const json& dumped, const string& name) {
  for (const auto& option : dumped["options"]) {
    if (option["name"] == name) {
      return option;
    }
  }
  BOOST_FAIL("No option " + name);
  return json();
}

// Reads the texts with the option's syntax and writes the value the way
// pcb2gcode writes values.
std::vector<string> reformat(const string& option, const std::vector<string>& texts) {
  const po::option_description& description = options::catalog().options.find(option, false);
  boost::any value;
  for (const auto& text : texts) {
    description.semantic()->parse(value, {text}, false);
  }
  const boost::optional<std::vector<string>> formatted = format_option_value(value);
  BOOST_REQUIRE(formatted);
  return *formatted;
}

// The text, or every text of a repeatable option, reads back unchanged.
void check_reads_back(const string& option, const json& text) {
  BOOST_TEST_CONTEXT("--" << option << "=" << text) {
    const std::vector<string> texts = text.is_array() ? text.get<std::vector<string>>()
                                                      : std::vector<string>{text.get<string>()};
    if (!texts.empty()) {
      BOOST_CHECK(reformat(option, texts) == texts);
    }
  }
}

// The values that the condition compares with read back unchanged.
void check_condition_literals(const json& condition) {
  if (condition.is_boolean()) {
    return;
  }
  for (const auto& operand : condition.items()) {
    if (operand.key() == "equal") {
      check_reads_back(operand.value()[0].get<string>(), operand.value()[1]);
    } else if (operand.key() == "not") {
      check_condition_literals(operand.value());
    } else if (operand.key() == "any" || operand.key() == "all") {
      for (const auto& sub_condition : operand.value()) {
        check_condition_literals(sub_condition);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(dump_lists_every_option) {
  const json dumped = dump();
  BOOST_CHECK_EQUAL(dumped["format_version"], 1);
  BOOST_CHECK_EQUAL(dumped["program"], "pcb2gcode");
  BOOST_CHECK(dumped["version"].is_string());
  std::set<string> group_ids;
  for (const auto& group : dumped["groups"]) {
    BOOST_CHECK(group_ids.insert(group["id"].get<string>()).second);
    BOOST_CHECK(!group["title"].get<string>().empty());
  }
  std::set<string> names;
  for (const auto& option : dumped["options"]) {
    BOOST_TEST_CONTEXT("--" << option["name"]) {
      BOOST_CHECK(names.insert(option["name"].get<string>()).second);
      for (const char* key : {"group", "description", "type", "default", "used_when"}) {
        BOOST_CHECK(option.contains(key));
      }
      BOOST_CHECK(group_ids.count(option["group"].get<string>()) == 1);
      BOOST_CHECK(dumped["types"].contains(option["type"].get<string>()));
      BOOST_CHECK(dumped["conditions"].contains(option["used_when"].get<string>()));
      if (option.contains("required_when")) {
        BOOST_CHECK(dumped["conditions"].contains(option["required_when"].get<string>()));
      }
    }
  }
  BOOST_CHECK_EQUAL(names.size(), options::catalog().options.options().size());
  std::set<int> codes;
  for (const auto& code : dumped["exit_codes"]) {
    codes.insert(code["code"].get<int>());
  }
  for (const auto& rule : dumped["rules"]) {
    BOOST_TEST_CONTEXT(rule["message"]) {
      BOOST_CHECK(dumped["conditions"].contains(rule["when"].get<string>()));
      BOOST_CHECK(codes.count(rule["code"].get<int>()) == 1);
      for (const auto& option : rule["options"]) {
        BOOST_CHECK(names.count(option.get<string>()) == 1);
      }
    }
  }
}

BOOST_AUTO_TEST_CASE(dump_values_read_back) {
  const json dumped = dump();
  for (const auto& option : dumped["options"]) {
    const string name = option["name"].get<string>();
    // pcb2gcode can't read inf, so the only way to get that default is to
    // leave the option out.
    if (!option["default"].is_null() && option["default"] != "inf") {
      check_reads_back(name, option["default"]);
    }
    if (option.contains("implicit")) {
      check_reads_back(name, option["implicit"]);
    }
  }
  for (const auto& rule : dumped["rules"]) {
    for (const char* key : {"min", "max", "example"}) {
      if (rule.contains(key)) {
        check_reads_back(rule["options"][0].get<string>(), rule[key]);
      }
    }
    if (rule.contains("if")) {
      check_condition_literals(rule["if"]);
    }
    if (rule.contains("unless")) {
      check_condition_literals(rule["unless"]);
    }
  }
  for (const auto& condition : dumped["conditions"]) {
    check_condition_literals(condition["expression"]);
  }
}

BOOST_AUTO_TEST_CASE(dump_types_read_back) {
  const json types = dump()["types"];
  for (const auto& text : types["bool"]["true"]) {
    BOOST_CHECK(reformat("metric", {boost::to_upper_copy(text.get<string>())}) ==
                std::vector<string>{"true"});
  }
  for (const auto& text : types["bool"]["false"]) {
    BOOST_CHECK(reformat("metric", {text.get<string>()}) == std::vector<string>{"false"});
  }
  // Every unit name reads back as the main name.
  for (const auto& type_and_option : std::vector<std::pair<string, string>>{
           {"length", "zsafe"}, {"time", "spinup-time"}, {"rpm", "mill-speed"},
           {"percent", "milldrill-stepover"}}) {
    for (const auto& unit : types[type_and_option.first]["units"]) {
      const string name = unit["name"].get<string>();
      std::vector<string> names = unit["aliases"].get<std::vector<string>>();
      names.push_back(name);
      for (const string& alias : names) {
        BOOST_TEST_CONTEXT("--" << type_and_option.second << "=2 " << alias) {
          BOOST_CHECK(reformat(type_and_option.second, {"2 " + alias}) ==
                      std::vector<string>{"2" + name});
        }
      }
    }
  }
  BOOST_CHECK(reformat("mill-feed", {"2 inches per minute"}) == std::vector<string>{"2in/min"});
  BOOST_CHECK(reformat("mill-speed", {"2 cycles/s"}) == std::vector<string>{"2rev/s"});
  BOOST_CHECK_EQUAL(types["length"]["units"][0]["name"], "mm");
  BOOST_CHECK_EQUAL(types["length"]["units"][0]["factor"], 0.001);
}

BOOST_AUTO_TEST_CASE(dump_option_details) {
  const json dumped = dump();

  const json zwork = find_option(dumped, "zwork");
  BOOST_CHECK_EQUAL(zwork["group"], "milling");
  BOOST_CHECK_EQUAL(zwork["type"], "length");
  BOOST_CHECK(zwork["default"].is_null());
  BOOST_CHECK_EQUAL(zwork["example"], "-0.05mm");
  BOOST_CHECK_EQUAL(zwork["required_when"], "milling");
  BOOST_CHECK_EQUAL(zwork["used_when"], "milling");

  const json optimise = find_option(dumped, "optimise");
  BOOST_CHECK_EQUAL(optimise["default"], "0.0001in");
  BOOST_CHECK_EQUAL(optimise["implicit"], "0.0001in");
  BOOST_CHECK_EQUAL(find_option(dumped, "g0-vertical-speed")["default"], "50in/min");
  BOOST_CHECK_EQUAL(find_option(dumped, "spinup-time")["default"], "1ms");
  BOOST_CHECK_EQUAL(find_option(dumped, "min-milldrill-hole-diameter")["default"], "inf");
  BOOST_CHECK_EQUAL(find_option(dumped, "milling-overlap")["type"], "length_or_percent");
  BOOST_CHECK_EQUAL(find_option(dumped, "milling-overlap")["default"], "50%");

  const json direction = find_option(dumped, "mill-feed-direction");
  BOOST_CHECK_EQUAL(direction["type"], "enum");
  BOOST_CHECK_EQUAL(direction["default"], "any");
  BOOST_CHECK_EQUAL(direction["choices"][1],
                    (json{{"value", "climb"}, {"aliases", json::array({"clockwise"})}}));
  BOOST_CHECK_EQUAL(find_option(dumped, "software")["choices"].size(), 4U);

  const json drills = find_option(dumped, "drills-available");
  BOOST_CHECK_EQUAL(drills["type"], "available_drill");
  BOOST_CHECK_EQUAL(drills["default"], json::array());
  BOOST_CHECK_EQUAL(drills["repeatable"], true);
  BOOST_CHECK_EQUAL(drills["comma_separated"], true);
  BOOST_CHECK_EQUAL(drills["multitoken"], true);
  BOOST_CHECK_EQUAL(find_option(dumped, "mill-diameters")["default"], json::array({"0"}));
  const json gcode = find_option(dumped, "pre-milling-gcode");
  BOOST_CHECK_EQUAL(gcode["repeatable"], true);
  BOOST_CHECK(!gcode.contains("comma_separated"));
  BOOST_CHECK(!gcode.contains("multitoken"));

  const json tolerance = find_option(dumped, "tolerance");
  BOOST_CHECK_EQUAL(tolerance["type"], "double");
  BOOST_CHECK(tolerance["default"].is_null());
  BOOST_CHECK_EQUAL(tolerance["bare_unit"], "in");
  BOOST_CHECK_EQUAL(tolerance["metric_bare_unit"], "mm");
  BOOST_CHECK_EQUAL(tolerance["range"], (json{{"min", "0"}}));

  const json help = find_option(dumped, "help");
  BOOST_CHECK_EQUAL(help["type"], "none");
  BOOST_CHECK_EQUAL(help["short"], "?");
  BOOST_CHECK_EQUAL(help["cli_only"], true);
  BOOST_CHECK_EQUAL(find_option(dumped, "version")["short"], "V");
  BOOST_CHECK_EQUAL(find_option(dumped, "dump-options")["cli_only"], true);
  BOOST_CHECK(!find_option(dumped, "zsafe").contains("cli_only"));

  const json drill_front = find_option(dumped, "drill-front");
  BOOST_CHECK_EQUAL(drill_front["deprecated"], true);
  BOOST_CHECK_EQUAL(drill_front["replaced_by"], "drill-side");
  BOOST_CHECK_EQUAL(drill_front["value_map"], (json{{"false", "back"}, {"true", "front"}}));
  BOOST_CHECK_EQUAL(drill_front["implicit"], "true");
  BOOST_CHECK_EQUAL(find_option(dumped, "g64")["replaced_by"], "tolerance");
  BOOST_CHECK(!find_option(dumped, "g64").contains("value_map"));
  // A number of passes doesn't carry over to a length.
  BOOST_CHECK_EQUAL(find_option(dumped, "extra-passes")["deprecated"], true);
  BOOST_CHECK(!find_option(dumped, "extra-passes").contains("replaced_by"));
  BOOST_CHECK_EQUAL(find_option(dumped, "vectorial")["deprecated"], true);
  BOOST_CHECK(!find_option(dumped, "mirror-yaxis").contains("implicit"));

  const json front = find_option(dumped, "front");
  BOOST_CHECK_EQUAL(front["path"], "input_file");
  BOOST_CHECK_EQUAL(front["file_format"], "gerber");
  BOOST_CHECK_EQUAL(front["used_when"], "always");
  BOOST_CHECK_EQUAL(find_option(dumped, "front-output")["relative_to"], "output-dir");
  const json manifest = find_option(dumped, "manifest");
  BOOST_CHECK_EQUAL(manifest["group"], "generic");
  BOOST_CHECK_EQUAL(manifest["type"], "string");
  BOOST_CHECK(manifest["default"].is_null());
  BOOST_CHECK_EQUAL(manifest["used_when"], "always");
  BOOST_CHECK_EQUAL(manifest["path"], "output_file");
  BOOST_CHECK_EQUAL(manifest["file_format"], "json");
  BOOST_CHECK_EQUAL(manifest["relative_to"], "output-dir");
  BOOST_CHECK_EQUAL(find_option(dumped, "output-dir")["path"], "directory");

  BOOST_CHECK_EQUAL(find_option(dumped, "cut-infeed")["range"], (json{{"min", "0.001in"}}));
  BOOST_CHECK_EQUAL(find_option(dumped, "tile-x")["range"], (json{{"min", "1"}}));
  const json stepover = find_option(dumped, "milldrill-stepover");
  BOOST_CHECK_EQUAL(stepover["range"],
                    (json{{"min", "0"}, {"min_exclusive", true}, {"max", "100%"}}));
  BOOST_CHECK_EQUAL(stepover["used_when"], "milldrilling");
}

BOOST_AUTO_TEST_CASE(dump_conditions) {
  const json conditions = dump()["conditions"];
  BOOST_CHECK_EQUAL(conditions["always"]["expression"], true);
  const json front = {{"set", "front"}};
  const json back = {{"set", "back"}};
  BOOST_CHECK_EQUAL(conditions["milling"]["expression"],
                    (json{{"any", json::array({front, back})}}));
  const json outline = {{"set", "outline"}};
  const json drill = {{"set", "drill"}};
  const json finite = {{"finite", "min-milldrill-hole-diameter"}};
  const json milldrilling = {{"all", json::array({drill, finite})}};
  BOOST_CHECK_EQUAL(conditions["milldrilling"]["expression"], milldrilling);
  BOOST_CHECK_EQUAL(conditions["cutting"]["expression"],
                    (json{{"any", json::array({outline, milldrilling})}}));
}

BOOST_AUTO_TEST_CASE(format_values) {
  BOOST_CHECK(reformat("pre-milling-gcode", {"M3", "G4 P1"}) ==
              (std::vector<string>{"M3", "G4 P1"}));
  BOOST_CHECK(reformat("drills-available", {"1mm:-0.1mm:+0.2mm,2mm", "3mm"}) ==
              (std::vector<string>{"1mm:-0.1mm:+0.2mm,2mm", "3mm"}));
  // pcb2gcode has no options of this type.
  BOOST_CHECK(!format_option_value(boost::any(1.5f)));
}

BOOST_AUTO_TEST_SUITE_END()
