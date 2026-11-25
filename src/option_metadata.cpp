/*
 * This file is part of pcb2gcode.
 *
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "option_metadata.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <typeindex>

#include <boost/algorithm/string.hpp>
#include <boost/variant.hpp>
#include <nlohmann/json.hpp>

#include "available_drills.hpp"
#include "pcb2gcode_version.hpp"
#include "units.hpp"

using json = nlohmann::ordered_json;
using std::string;
using std::vector;

namespace {

// Bump only for changes that break readers.  Adding keys doesn't.
constexpr int FORMAT_VERSION = 1;

// What bare numbers mean for lengths and velocities: these units, or the
// metric ones with --metric.
const char* const BARE_LENGTH = "in";
const char* const METRIC_BARE_LENGTH = "mm";
const char* const BARE_VELOCITY = "in/min";
const char* const METRIC_BARE_VELOCITY = "mm/min";

// How pcb2gcode reads and writes the values of one C++ type.
struct ValueType {
  string name;
  bool repeatable;
  bool comma_separated;
  // The value in input syntax, one string per occurrence.
  std::function<vector<string>(const boost::any&)> format;
  // The value as a number in the units that pcb2gcode works in (inches,
  // inches/minute, milliseconds, rpm, fractions), for comparisons.
  std::function<boost::optional<double>(const boost::any&, double unit)> number;
  // For enums.
  json choices;
};

string format_value(const boost::variant<Length, Percent>& value) {
  return boost::apply_visitor([](const auto& alternative) { return to_input_syntax(alternative); },
                              value);
}

template <typename T>
string format_value(const T& value) {
  if constexpr (std::is_same_v<T, bool>) {
    return value ? "true" : "false";
  } else if constexpr (std::is_integral_v<T>) {
    return std::to_string(value);
  } else if constexpr (std::is_floating_point_v<T>) {
    return format_number(value);
  } else if constexpr (std::is_same_v<T, string>) {
    return value;
  } else if constexpr (std::is_enum_v<T>) {
    std::ostringstream text;
    text << value;
    return text.str();
  } else {
    return to_input_syntax(value);
  }
}

template <typename T>
boost::optional<double> number_value(const T& value, double unit) {
  if constexpr (std::is_same_v<T, Length>) {
    return value.asInch(unit);
  } else if constexpr (std::is_same_v<T, Velocity>) {
    return value.asInchPerMinute(unit);
  } else if constexpr (std::is_same_v<T, Time>) {
    return value.asMillisecond(1);
  } else if constexpr (std::is_same_v<T, Rpm>) {
    return value.asRpm(1);
  } else if constexpr (std::is_same_v<T, Percent>) {
    return value.asFraction(1);
  } else if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
    return static_cast<double>(value);
  } else {
    return boost::none;
  }
}

template <typename T>
ValueType scalar_type(const string& name) {
  return ValueType{
    name, false, false,
    [](const boost::any& value) {
      return vector<string>{format_value(boost::any_cast<const T&>(value))};
    },
    [](const boost::any& value, double unit) {
      return number_value(boost::any_cast<const T&>(value), unit);
    },
    json()};
}

template <typename T>
ValueType enum_type() {
  ValueType type = scalar_type<T>("enum");
  type.choices = json::array();
  for (const auto& choice : enum_choices<T>()) {
    type.choices.push_back({
        {"value", choice.names.front()},
        {"aliases", vector<string>(choice.names.cbegin() + 1, choice.names.cend())}});
  }
  return type;
}

// Given several times, each time with items separated by commas.
template <typename T>
ValueType comma_separated_type(const string& name) {
  return ValueType{
    name, true, true,
    [](const boost::any& value) {
      vector<string> occurrences;
      for (const auto& occurrence : boost::any_cast<const vector<CommaSeparated<T>>&>(value)) {
        vector<string> items;
        for (const auto& item : flatten(vector<CommaSeparated<T>>{occurrence})) {
          items.push_back(format_value(item));
        }
        occurrences.push_back(boost::algorithm::join(items, ","));
      }
      return occurrences;
    },
    [](const boost::any&, double) -> boost::optional<double> { return boost::none; },
    json()};
}

template <typename T>
ValueType repeated_type(const string& name) {
  return ValueType{
    name, true, false,
    [](const boost::any& value) {
      vector<string> occurrences;
      for (const auto& occurrence : boost::any_cast<const vector<T>&>(value)) {
        occurrences.push_back(format_value(occurrence));
      }
      return occurrences;
    },
    [](const boost::any&, double) -> boost::optional<double> { return boost::none; },
    json()};
}

// Every type that options are declared with.
const std::map<std::type_index, ValueType>& value_types() {
  static const std::map<std::type_index, ValueType> types{
    {typeid(bool), scalar_type<bool>("bool")},
    {typeid(int), scalar_type<int>("int")},
    {typeid(unsigned int), scalar_type<unsigned int>("uint")},
    {typeid(size_t), scalar_type<size_t>("uint")},
    {typeid(double), scalar_type<double>("double")},
    {typeid(string), scalar_type<string>("string")},
    {typeid(Length), scalar_type<Length>("length")},
    {typeid(Time), scalar_type<Time>("time")},
    {typeid(Velocity), scalar_type<Velocity>("velocity")},
    {typeid(Rpm), scalar_type<Rpm>("rpm")},
    {typeid(Percent), scalar_type<Percent>("percent")},
    {typeid(boost::variant<Length, Percent>),
     scalar_type<boost::variant<Length, Percent>>("length_or_percent")},
    {typeid(BoardSide::BoardSide), enum_type<BoardSide::BoardSide>()},
    {typeid(Software::Software), enum_type<Software::Software>()},
    {typeid(MillFeedDirection::MillFeedDirection),
     enum_type<MillFeedDirection::MillFeedDirection>()},
    {typeid(vector<CommaSeparated<string>>), comma_separated_type<string>("string")},
    {typeid(vector<CommaSeparated<Length>>), comma_separated_type<Length>("length")},
    {typeid(vector<AvailableDrills>), comma_separated_type<AvailableDrill>("available_drill")},
    {typeid(vector<string>), repeated_type<string>("string")},
  };
  return types;
}

const ValueType* find_value_type(const std::type_info& type) {
  const auto found = value_types().find(type);
  return found == value_types().cend() ? nullptr : &found->second;
}

const ValueType* find_value_type(const po::option_description& option) {
  const auto* typed = dynamic_cast<const po::typed_value_base*>(option.semantic().get());
  return typed ? find_value_type(typed->value_type()) : nullptr;
}

double number_of(const boost::any& value, double unit) {
  const ValueType* type = find_value_type(value.type());
  const boost::optional<double> number = type ? type->number(value, unit) : boost::none;
  if (!number) {
    throw std::logic_error("Can't compare an option of this type");
  }
  return *number;
}

json units_json(const vector<string>& names, double factor) {
  return {{"name", names.front()},
          {"aliases", vector<string>(names.cbegin() + 1, names.cend())},
          {"factor", factor}};
}

template <typename unit_t>
json named_units_json() {
  json units = json::array();
  for (const auto& unit : unit_t::named_units()) {
    units.push_back(units_json(unit.names, unit.quantity.value()));
  }
  return units;
}

json types_json() {
  json types;
  types["none"] = {{"description", "Takes no value, like --help."}};
  // The values that Boost accepts for a bool, ignoring case.
  types["bool"] = {{"description", "true or false, ignoring case."},
                   {"true", {"true", "yes", "on", "1"}},
                   {"false", {"false", "no", "off", "0"}}};
  types["int"] = {{"description", "A whole number."}};
  types["uint"] = {{"description", "A whole number, at least 0."}};
  types["double"] = {{"description", "A number."}};
  types["string"] = {{"description", "Any text."}};
  types["length"] = {
    {"description", "A number with an optional unit."},
    {"base_unit", "m"},
    {"bare_unit", BARE_LENGTH},
    {"metric_bare_unit", METRIC_BARE_LENGTH},
    {"units", named_units_json<Length>()}};
  types["time"] = {
    {"description", "A number with an optional unit."},
    {"base_unit", "s"},
    {"bare_unit", "ms"},
    {"units", named_units_json<Time>()}};
  types["velocity"] = {
    {"description", "A number with an optional length unit, separator and time unit, like "
                    "600mm/min or 1 inch per second."},
    {"base_unit", "m/s"},
    {"bare_unit", BARE_VELOCITY},
    {"metric_bare_unit", METRIC_BARE_VELOCITY},
    {"ratio_of", {"length", "time"}},
    {"separators", Lexer::divisions()}};
  types["revolution"] = {
    {"description", "Not an option type: the numerator of rpm ratios."},
    {"base_unit", "rev"},
    {"units", named_units_json<Revolution>()}};
  types["rpm"] = {
    {"description", "A number with an optional unit, either rpm or a revolution unit, separator "
                    "and time unit, like 2 rev/s."},
    {"base_unit", "rpm"},
    {"bare_unit", "rpm"},
    {"units", named_units_json<Rpm>()},
    {"ratio_of", {"revolution", "time"}},
    {"separators", Lexer::divisions()}};
  types["percent"] = {
    {"description", "A number with an optional %.  A bare number is a fraction: 0.5 is 50%."},
    {"base_unit", "%"},
    {"bare_unit", "fraction"},
    {"units", named_units_json<Percent>()}};
  types["length_or_percent"] = {{"description", "A length or a percent."},
                                {"one_of", {"length", "percent"}}};
  types["enum"] = {{"description", "One of the option's choices or their aliases, ignoring "
                                   "case."}};
  types["available_drill"] = {
    {"description", "A drill diameter, optionally with the tolerances of the holes that it "
                    "drills, all lengths.  With one tolerance, it's used for both."},
    {"syntax", "<diameter>[:<negative tolerance>[:+<positive tolerance>]]"}};
  return types;
}

// Outside of any function, so that coverage tools don't map each macro
// expansion as a region of code.
constexpr std::pair<int, const char*> EXIT_CODES[] = {
#define PCB2GCODE_ERROR_CODE_PAIR(name, value) {value, #name},
  PCB2GCODE_ERROR_CODES(PCB2GCODE_ERROR_CODE_PAIR)
#undef PCB2GCODE_ERROR_CODE_PAIR
};

json exit_codes_json() {
  json codes = json::array();
  for (const auto& exit_code : EXIT_CODES) {
    codes.push_back({{"code", exit_code.first}, {"name", exit_code.second}});
  }
  return codes;
}

json option_json(const OptionCatalog& catalog, const OptionGroup& group, const string& name,
                 const json& rules) {
  const po::option_description* option = catalog.options.find_nothrow(name, false);
  if (option == nullptr) {
    throw std::logic_error("Unknown option " + name);
  }
  const auto found_details = catalog.details.find(name);
  const OptionDetails details =
      found_details == catalog.details.cend() ? OptionDetails() : found_details->second;
  const ValueType* type = find_value_type(*option);
  const auto& semantic = *option->semantic();

  json result;
  result["name"] = name;
  const string display_name =
      option->canonical_display_name(po::command_line_style::allow_dash_for_short);
  if (display_name.size() == 2 && display_name[0] == '-') {
    result["short"] = display_name.substr(1);
  }
  result["group"] = group.id;
  result["description"] = option->description();
  if (type != nullptr) {
    result["type"] = type->name;
  } else {
    result["type"] = semantic.max_tokens() == 0 ? "none" : "unknown";
  }
  boost::any value;
  if (type != nullptr && semantic.apply_default(value)) {
    const vector<string> text = type->format(value);
    result["default"] = type->repeatable ? json(text) : json(text.front());
  } else {
    result["default"] = nullptr;
  }
  if (!details.default_note.empty()) {
    result["default_note"] = details.default_note;
  }
  if (type != nullptr && semantic.min_tokens() == 0) {
    boost::any implicit;
    semantic.parse(implicit, {}, false);
    const vector<string> text = type->format(implicit);
    result["implicit"] = type->repeatable ? json(text) : json(text.front());
  }
  if (type != nullptr && type->repeatable) {
    result["repeatable"] = true;
  }
  if (type != nullptr && type->comma_separated) {
    result["comma_separated"] = true;
  }
  if (semantic.max_tokens() > 1) {
    result["multitoken"] = true;
  }
  if (type != nullptr && !type->choices.is_null()) {
    result["choices"] = type->choices;
  }
  if (details.bare_length) {
    result["bare_unit"] = BARE_LENGTH;
    result["metric_bare_unit"] = METRIC_BARE_LENGTH;
  }
  for (const auto& rule : rules) {
    if (rule["options"] != json::array({name})) {
      continue;
    }
    if (rule["kind"] == "range") {
      json range;
      for (const char* key : {"min", "min_exclusive", "max", "max_exclusive"}) {
        if (rule.contains(key)) {
          range[key] = rule[key];
        }
      }
      result["range"] = range;
    } else if (rule["kind"] == "required") {
      result["example"] = rule["example"];
      result["required_when"] = rule["when"];
    }
  }
  result["used_when"] = details.used_when.empty() ? group.used_when : details.used_when;
  if (group.cli_only) {
    result["cli_only"] = true;
  }
  if (boost::starts_with(option->description(), "[DEPRECATED")) {
    result["deprecated"] = true;
  }
  if (!details.replaced_by.empty()) {
    result["replaced_by"] = details.replaced_by;
    if (!details.value_map.empty()) {
      result["value_map"] = details.value_map;
    }
  }
  if (!details.path.empty()) {
    result["path"] = details.path;
    if (!details.file_format.empty()) {
      result["file_format"] = details.file_format;
    }
    if (details.path == "output_file") {
      result["relative_to"] = "output-dir";
    }
  }
  return result;
}

} // namespace

OptionCondition::OptionCondition() : kind(Kind::ALWAYS) {}

OptionCondition::OptionCondition(Kind kind, const string& option, const string& value,
                                 const vector<OptionCondition>& operands)
    : kind(kind), option(option), value(value), operands(operands) {}

OptionCondition OptionCondition::set(const string& option) {
  return OptionCondition(Kind::SET, option, "", {});
}

OptionCondition OptionCondition::given(const string& option) {
  return OptionCondition(Kind::GIVEN, option, "", {});
}

OptionCondition OptionCondition::equal(const string& option, const string& value) {
  return OptionCondition(Kind::EQUAL, option, value, {});
}

OptionCondition OptionCondition::finite(const string& option) {
  return OptionCondition(Kind::FINITE, option, "", {});
}

OptionCondition OptionCondition::operator!() const {
  return OptionCondition(Kind::NOT, "", "", {*this});
}

// Keeps a || b || c flat instead of nesting it.
OptionCondition OptionCondition::combine(Kind kind, const OptionCondition& a,
                                         const OptionCondition& b) {
  vector<OptionCondition> operands;
  for (const OptionCondition* condition : {&a, &b}) {
    if (condition->kind == kind) {
      operands.insert(operands.end(), condition->operands.cbegin(), condition->operands.cend());
    } else {
      operands.push_back(*condition);
    }
  }
  return OptionCondition(kind, "", "", operands);
}

OptionCondition OptionCondition::operator&&(const OptionCondition& other) const {
  return combine(Kind::ALL, *this, other);
}

OptionCondition OptionCondition::operator||(const OptionCondition& other) const {
  return combine(Kind::ANY, *this, other);
}

bool OptionCondition::holds(const po::variables_map& vm) const {
  switch (kind) {
    case Kind::ALWAYS:
      return true;
    case Kind::SET:
      return vm.count(option) > 0;
    case Kind::GIVEN:
      return vm.count(option) > 0 && !vm[option].defaulted();
    case Kind::EQUAL: {
      if (vm.count(option) == 0) {
        return false;
      }
      const auto text = format_option_value(vm[option].value());
      return text && *text == vector<string>{value};
    }
    case Kind::FINITE:
      return vm.count(option) > 0 && std::isfinite(number_of(vm[option].value(), 1));
    case Kind::NOT:
      return !operands.front().holds(vm);
    case Kind::ANY:
      return std::any_of(operands.cbegin(), operands.cend(),
                         [&](const OptionCondition& operand) { return operand.holds(vm); });
    case Kind::ALL:
      return std::all_of(operands.cbegin(), operands.cend(),
                         [&](const OptionCondition& operand) { return operand.holds(vm); });
  }
  return false;
}

json OptionCondition::to_json() const {
  switch (kind) {
    case Kind::ALWAYS:
      return true;
    case Kind::SET:
      return {{"set", option}};
    case Kind::GIVEN:
      return {{"given", option}};
    case Kind::EQUAL:
      return {{"equal", json::array({option, value})}};
    case Kind::FINITE:
      return {{"finite", option}};
    case Kind::NOT:
      return {{"not", operands.front().to_json()}};
    case Kind::ANY:
    case Kind::ALL: {
      json list = json::array();
      for (const auto& operand : operands) {
        list.push_back(operand.to_json());
      }
      return {{kind == Kind::ANY ? "any" : "all", list}};
    }
  }
  return nullptr;
}

OptionDetails& OptionDetails::when(const string& condition) {
  used_when = condition;
  return *this;
}

OptionDetails& OptionDetails::input(const string& format) {
  path = "input_file";
  file_format = format;
  return *this;
}

OptionDetails& OptionDetails::output(const string& format) {
  path = "output_file";
  file_format = format;
  return *this;
}

OptionDetails& OptionDetails::directory() {
  path = "directory";
  return *this;
}

OptionDetails& OptionDetails::note(const string& text) {
  default_note = text;
  return *this;
}

OptionDetails& OptionDetails::replaced(const string& option,
                                       const std::map<string, string>& values) {
  replaced_by = option;
  value_map = values;
  return *this;
}

OptionDetails& OptionDetails::length_number() {
  bare_length = true;
  return *this;
}

OptionRule::OptionRule(Kind kind, const vector<string>& options)
    : kind(kind), options_(options), when_("always"), code_(ERR_INVALIDPARAMETER) {}

OptionRule OptionRule::required(const string& option, const string& description,
                                const string& example) {
  OptionRule rule(Kind::REQUIRED, {option});
  rule.description_ = description;
  rule.example_ = example;
  // Examples have units so that they work with and without --metric.
  rule.message_ = "Missing --" + option + ", " + description + ". For example: --" + option + "=" +
                  example;
  return rule;
}

OptionRule OptionRule::range(const string& option) {
  return OptionRule(Kind::RANGE, {option});
}

OptionRule OptionRule::greater(const string& option, const string& other) {
  return OptionRule(Kind::GREATER, {option, other});
}

OptionRule OptionRule::greater_or_equal(const string& option, const string& other) {
  return OptionRule(Kind::GREATER_OR_EQUAL, {option, other});
}

OptionRule OptionRule::forbidden(const OptionCondition& condition, const vector<string>& options) {
  OptionRule rule(Kind::FORBIDDEN, options);
  rule.if_ = condition;
  return rule;
}

OptionRule& OptionRule::at_least(const string& limit) {
  min_ = Limit{limit, false};
  return *this;
}

OptionRule& OptionRule::above(const string& limit) {
  min_ = Limit{limit, true};
  return *this;
}

OptionRule& OptionRule::at_most(const string& limit) {
  max_ = Limit{limit, false};
  return *this;
}

OptionRule& OptionRule::below(const string& limit) {
  max_ = Limit{limit, true};
  return *this;
}

OptionRule& OptionRule::when(const string& condition) {
  when_ = condition;
  return *this;
}

OptionRule& OptionRule::unless(const OptionCondition& condition) {
  unless_ = condition;
  return *this;
}

OptionRule& OptionRule::fails_with(ErrorCodes code, const string& message) {
  code_ = code;
  if (!message.empty()) {
    message_ = message;
  }
  return *this;
}

// Reads the limit with the syntax of the option that it limits.
double OptionRule::limit_value(const Limit& limit, const OptionCatalog& catalog,
                               double unit) const {
  const po::option_description* option = catalog.options.find_nothrow(options_.front(), false);
  if (option == nullptr) {
    throw std::logic_error("Unknown option " + options_.front());
  }
  boost::any value;
  option->semantic()->parse(value, {limit.value}, false);
  return number_of(value, unit);
}

boost::optional<OptionError> OptionRule::check(const po::variables_map& vm,
                                               const OptionCatalog& catalog, double unit) const {
  if (!catalog.condition(when_).holds(vm)) {
    return boost::none;
  }
  const OptionError error{message_, code_, options_};
  switch (kind) {
    case Kind::REQUIRED:
      if (vm.count(options_[0]) == 0) {
        return error;
      }
      break;
    case Kind::RANGE: {
      if (vm.count(options_[0]) == 0) {
        break;
      }
      const double value = number_of(vm[options_[0]].value(), unit);
      if (min_) {
        const double limit = limit_value(*min_, catalog, unit);
        if (min_->exclusive ? !(value > limit) : !(value >= limit)) {
          return error;
        }
      }
      if (max_) {
        const double limit = limit_value(*max_, catalog, unit);
        if (max_->exclusive ? !(value < limit) : !(value <= limit)) {
          return error;
        }
      }
      break;
    }
    case Kind::GREATER:
    case Kind::GREATER_OR_EQUAL: {
      if (vm.count(options_[0]) == 0 || vm.count(options_[1]) == 0 ||
          (unless_ && unless_->holds(vm))) {
        break;
      }
      const double a = number_of(vm[options_[0]].value(), unit);
      const double b = number_of(vm[options_[1]].value(), unit);
      if (kind == Kind::GREATER ? !(a > b) : !(a >= b)) {
        return error;
      }
      break;
    }
    case Kind::FORBIDDEN:
      if (if_.holds(vm)) {
        return error;
      }
      break;
  }
  return boost::none;
}

json OptionRule::to_json() const {
  json rule;
  switch (kind) {
    case Kind::REQUIRED:
      rule["kind"] = "required";
      break;
    case Kind::RANGE:
      rule["kind"] = "range";
      break;
    case Kind::GREATER:
      rule["kind"] = "greater";
      break;
    case Kind::GREATER_OR_EQUAL:
      rule["kind"] = "greater_or_equal";
      break;
    case Kind::FORBIDDEN:
      rule["kind"] = "forbidden";
      break;
  }
  rule["options"] = options_;
  rule["when"] = when_;
  if (kind == Kind::REQUIRED) {
    rule["description"] = description_;
    rule["example"] = example_;
  }
  if (min_) {
    rule["min"] = min_->value;
    if (min_->exclusive) {
      rule["min_exclusive"] = true;
    }
  }
  if (max_) {
    rule["max"] = max_->value;
    if (max_->exclusive) {
      rule["max_exclusive"] = true;
    }
  }
  if (unless_) {
    rule["unless"] = unless_->to_json();
  }
  if (kind == Kind::FORBIDDEN) {
    rule["if"] = if_.to_json();
  }
  rule["code"] = code_;
  rule["message"] = message_;
  return rule;
}

const OptionCondition& OptionCatalog::condition(const string& name) const {
  for (const auto& condition : conditions) {
    if (condition.name == name) {
      return condition.condition;
    }
  }
  throw std::logic_error("Unknown condition " + name);
}

void check_rules(const OptionCatalog& catalog, const po::variables_map& vm,
                 vector<OptionError>& errors) {
  const double unit = vm["metric"].as<bool>() ? (1. / 25.4) : 1;
  for (const auto& rule : catalog.rules) {
    const boost::optional<OptionError> error = rule.check(vm, catalog, unit);
    if (error) {
      errors.push_back(*error);
    }
  }
}

string dump_catalog(const OptionCatalog& catalog) {
  json dump;
  dump["format_version"] = FORMAT_VERSION;
  dump["program"] = PACKAGE_NAME;
  dump["version"] = PCB2GCODE_PROJECT_VERSION;
#ifdef GIT_VERSION
  dump["git_commit"] = GIT_VERSION;
#else
  dump["git_commit"] = nullptr;
#endif
  dump["types"] = types_json();
  json conditions = json::object();
  for (const auto& condition : catalog.conditions) {
    conditions[condition.name] = {{"description", condition.description},
                                  {"expression", condition.condition.to_json()}};
  }
  dump["conditions"] = conditions;
  json groups = json::array();
  for (const auto& group : catalog.groups) {
    groups.push_back({{"id", group.id}, {"title", group.title}});
  }
  dump["groups"] = groups;
  json rules = json::array();
  for (const auto& rule : catalog.rules) {
    rules.push_back(rule.to_json());
  }
  json options = json::array();
  for (const auto& group : catalog.groups) {
    for (const auto& name : group.options) {
      options.push_back(option_json(catalog, group, name, rules));
    }
  }
  dump["options"] = options;
  dump["rules"] = rules;
  dump["exit_codes"] = exit_codes_json();
  return dump.dump(2);
}

boost::optional<vector<string>> format_option_value(const boost::any& value) {
  const ValueType* type = find_value_type(value.type());
  if (type == nullptr) {
    return boost::none;
  }
  return type->format(value);
}
