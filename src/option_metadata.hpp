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

#ifndef OPTION_METADATA_HPP
#define OPTION_METADATA_HPP

#include <map>
#include <string>
#include <vector>

#include <boost/any.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <nlohmann/json_fwd.hpp>

#include "options.hpp"

// A condition on the option values, like "--front or --back is set".
// pcb2gcode uses it to decide which checks apply and --dump-options writes it
// out, so that a GUI can make the same decisions.
class OptionCondition {
 public:
  // Always true.
  OptionCondition();
  // The option has a value, maybe its default.
  static OptionCondition set(const std::string& option);
  // The option was given on the command line or in a config file.
  static OptionCondition given(const std::string& option);
  // The option's value, as pcb2gcode writes it (like "true" or "any"), is
  // value.
  static OptionCondition equal(const std::string& option, const std::string& value);
  // The option has a value and it isn't infinite.
  static OptionCondition finite(const std::string& option);
  OptionCondition operator!() const;
  OptionCondition operator&&(const OptionCondition& other) const;
  OptionCondition operator||(const OptionCondition& other) const;

  bool holds(const po::variables_map& vm) const;
  nlohmann::ordered_json to_json() const;

 private:
  enum class Kind { ALWAYS, SET, GIVEN, EQUAL, FINITE, NOT, ANY, ALL };
  OptionCondition(Kind kind, const std::string& option, const std::string& value,
                  const std::vector<OptionCondition>& operands);
  static OptionCondition combine(Kind kind, const OptionCondition& a, const OptionCondition& b);
  Kind kind;
  std::string option;
  std::string value;
  std::vector<OptionCondition> operands;
};

// A condition that options and rules refer to by name, like "milling".
struct NamedCondition {
  std::string name;
  std::string description;
  OptionCondition condition;
};

// What --dump-options says about an option beyond its declaration.
struct OptionDetails {
  // pcb2gcode uses the option only while this named condition holds, instead
  // of when its group's condition holds.
  OptionDetails& when(const std::string& condition);
  OptionDetails& input(const std::string& format);
  // An output file name, relative to --output-dir.
  OptionDetails& output(const std::string& format);
  OptionDetails& directory();
  // Explains a default that isn't a plain value, like "half of --mill-feed".
  OptionDetails& note(const std::string& text);
  // Without value_map, the value carries over unchanged.
  OptionDetails& replaced(const std::string& option,
                          const std::map<std::string, std::string>& values = {});
  // A plain number that pcb2gcode reads as a length, in inches or, with
  // --metric, in millimeters.
  OptionDetails& length_number();

  std::string used_when;
  std::string path;
  std::string file_format;
  std::string default_note;
  std::string replaced_by;
  std::map<std::string, std::string> value_map;
  bool bare_length = false;
};

struct OptionError {
  std::string what;
  ErrorCodes code;
  // The options that the failed rule involves.
  std::vector<std::string> options;
  // The run needs the missing value, so --ignore-warnings can't skip this.
  bool needed = false;
};

struct OptionCatalog;

// A check that pcb2gcode runs on the option values before doing any work.
// It knows the options involved, the message and the exit code, so a
// validate-only mode can report failures as they are.
class OptionRule {
 public:
  // Fails if the option has no value.
  static OptionRule required(const std::string& option, const std::string& description,
                             const std::string& example);
  // Fails if the option has a value outside the limits that at_least(),
  // above(), at_most() and below() set.
  static OptionRule range(const std::string& option);
  // Fails if both options have values and option isn't greater than other.
  static OptionRule greater(const std::string& option, const std::string& other);
  static OptionRule greater_or_equal(const std::string& option, const std::string& other);
  // Fails if the condition holds.
  static OptionRule forbidden(const OptionCondition& condition,
                              const std::vector<std::string>& options);

  // Limits in pcb2gcode's input syntax, like "0.001in" or "100%".
  OptionRule& at_least(const std::string& limit);
  OptionRule& above(const std::string& limit);
  OptionRule& at_most(const std::string& limit);
  OptionRule& below(const std::string& limit);
  // The rule only applies while this named condition holds.
  OptionRule& when(const std::string& condition);
  // The rule doesn't apply while this holds.
  OptionRule& unless(const OptionCondition& condition);
  // A required value is needed only while this holds, instead of whenever the
  // rule applies.
  OptionRule& needed_when(const OptionCondition& condition);
  // Required rules make their own message.
  OptionRule& fails_with(ErrorCodes code, const std::string& message = "");

  // unit is the factor for bare lengths and velocities: 1 for inches or
  // 1/25.4 for --metric.
  boost::optional<OptionError> check(const po::variables_map& vm, const OptionCatalog& catalog,
                                     double unit) const;
  nlohmann::ordered_json to_json() const;

 private:
  enum class Kind { REQUIRED, RANGE, GREATER, GREATER_OR_EQUAL, FORBIDDEN };
  struct Limit {
    std::string value;
    bool exclusive;
  };
  OptionRule(Kind kind, const std::vector<std::string>& options);
  double limit_value(const Limit& limit, const OptionCatalog& catalog, double unit) const;
  Kind kind;
  std::vector<std::string> options_;
  std::string when_;
  std::string description_;
  std::string example_;
  boost::optional<Limit> min_;
  boost::optional<Limit> max_;
  boost::optional<OptionCondition> unless_;
  boost::optional<OptionCondition> needed_;
  OptionCondition if_;
  ErrorCodes code_;
  std::string message_;
};

// Everything that pcb2gcode knows about its options.
struct OptionCatalog {
  // Every option.
  const po::options_description& options;
  // In --help order.
  const std::vector<OptionGroup>& groups;
  const std::vector<NamedCondition>& conditions;
  const std::map<std::string, OptionDetails>& details;
  // In the order that pcb2gcode checks them.
  const std::vector<OptionRule>& rules;

  const OptionCondition& condition(const std::string& name) const;
};

// Appends the failure of each rule, in order.
void check_rules(const OptionCatalog& catalog, const po::variables_map& vm,
                 std::vector<OptionError>& errors);

// The catalog as JSON, for --dump-options.
std::string dump_catalog(const OptionCatalog& catalog);

// The value as pcb2gcode reads it, one string per occurrence, or none if the
// type is unknown.
boost::optional<std::vector<std::string>> format_option_value(const boost::any& value);

#endif // OPTION_METADATA_HPP
