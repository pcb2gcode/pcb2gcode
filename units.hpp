#ifndef UNITS_HPP
#define UNITS_HPP

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/units/quantity.hpp>
#include <boost/optional.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/base_units/metric/minute.hpp>
#include <boost/units/base_units/imperial/inch.hpp>

namespace {

// String parsers: Each on uses characters from the front of the
// string and leaves the unused characters in place.
struct parse_exception : public std::invalid_argument {
  parse_exception(const std::string& what) : std::invalid_argument(what) {}
};

void get_whitespace(std::string& input) {
  std::istringstream stream(input);
  stream >> std::ws;
  std::string new_input;
  getline(stream, new_input);
  input = new_input;
}

// Gets the double from the start of the string which may start with
// whitespace.  Throws an exception if it fails.
double get_double(std::string& input) {
  get_whitespace(input);
  double result;
  std::istringstream stream(input);
  if (stream >> result) {
    std::string new_input;
    getline(stream, new_input);
    input = new_input;
    return result;
  } else {
    throw parse_exception("Can't get double in: " + input);
  }
}

// Get the word from the start of the string which may have spaces.
// All characters up to the first that is not std::isalnum are
// returned.  They are removed from the input.  The result might have
// length 0.
std::string get_word(std::string& input) {
  get_whitespace(input);
  auto end = input.begin();
  while (end != input.end() && std::isalnum(*end))
    end++;
  std::string result(input.begin(), end);
  input.erase(input.begin(), end);
  return result;
}

// Get division indicator from the start of the string, either a slash
// or "per", which must have no spaces.  The division character is
// removed from the input.  Throws if it can't find the division
// character.
void get_division(std::string& input) {
  get_whitespace(input);
  if (input.compare(0, 1, "/") == 0) {
    input.erase(0, 1);
    return;
  }
  if (input.substr(0, 3) == "per") {
    input.erase(0, 3);
    return;
  }
  throw parse_exception("Can't get division character in: " + input);
}

template <typename dimension_t>
class UnitBase {
 public:
  UnitBase(double value, boost::optional<boost::units::quantity<dimension_t>> one) : value(value), one(one) {}
  double as(double factor, boost::units::quantity<dimension_t> wanted_unit) const {
    if (!one) {
      // We don't know the units so just use whatever factor was supplied.
      return value*factor;
    }
    return value*(*one)/wanted_unit;
  }

  double asDouble() const {
    return value;
  }
 protected:
  double value;
  boost::optional<boost::units::quantity<dimension_t>> one;
};

// dimension_t is "length" or "velocity", for example.
template<typename dimension_t>
class Unit : public UnitBase<dimension_t> {};

// Any non-SI base units that you want to use go here.
const boost::units::quantity<boost::units::si::length> inch = (boost::units::conversion_factor(boost::units::imperial::inch_base_unit::unit_type(),
                                                                                               boost::units::si::meter) * boost::units::si::meter);
const boost::units::quantity<boost::units::si::time> minute = (boost::units::conversion_factor(boost::units::metric::minute_base_unit::unit_type(),
                                                                                               boost::units::si::second) * boost::units::si::second);

// shortcuts for Units defined below.
typedef Unit<boost::units::si::length> Length;
typedef Unit<boost::units::si::time> Time;
typedef Unit<boost::units::si::dimensionless> Dimensionless;
typedef Unit<boost::units::si::velocity> Velocity;
typedef Unit<boost::units::si::frequency> Frequency;

template<>
class Unit<boost::units::si::length> : public UnitBase<boost::units::si::length> {
 public:
  Unit(double value, boost::optional<boost::units::quantity<boost::units::si::length>> one) : UnitBase(value, one) {}
  double asInch(double factor) const {
    return as(factor, inch);
  }
  static boost::units::quantity<boost::units::si::length> get_unit(const std::string& s) {
    std::string argument(s);
    std::string unit = get_word(argument);
    if (unit == "mm" ||
        unit == "millimeter" ||
        unit == "millimeters") {
      return boost::units::si::meter/1000.0;
    }
    if (unit == "in" ||
        unit == "inch" ||
        unit == "inches") {
      return inch;
    }
    std::cerr << "Didn't recognize units of length: " << unit << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }
};

template<>
class Unit<boost::units::si::time> : public UnitBase<boost::units::si::time> {
 public:
  Unit(double value, boost::optional<boost::units::quantity<boost::units::si::time>> one) : UnitBase(value, one) {}
  double asSecond(double factor) const {
    return as(factor, 1.0*boost::units::si::second);
  }
  static boost::units::quantity<boost::units::si::time> get_unit(const std::string& s) {
    if (s == "s" ||
        s == "second" ||
        s == "seconds") {
      return 1.0*boost::units::si::second;
    }
    if (s == "min" ||
        s == "mins" ||
        s == "minute" ||
        s == "minutes") {
      return minute;
    }
    std::cerr << "Didn't recognize units of time: " << s << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }
};

template<>
class Unit<boost::units::si::dimensionless> : public UnitBase<boost::units::si::dimensionless> {
 public:
  Unit(double value, boost::optional<boost::units::quantity<boost::units::si::dimensionless>> one) : UnitBase(value, one) {}
  using UnitBase::as;
  double as(double factor) const {
    return as(factor, 1.0*boost::units::si::si_dimensionless);
  }
  static boost::units::quantity<boost::units::si::dimensionless> get_unit(const std::string& s) {
    if (s == "rotation" ||
        s == "rotations" ||
        s == "cycle" ||
        s == "cycles") {
      return 1.0*boost::units::si::si_dimensionless;
    }
    std::cerr << "Didn't recognize dimensionless units: " << s << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }
};

template<>
class Unit<boost::units::si::velocity> : public UnitBase<boost::units::si::velocity> {
 public:
  Unit(double value, boost::optional<boost::units::quantity<boost::units::si::velocity>> one) : UnitBase(value, one) {}
  double asInchPerMinute(double factor) const {
    return as(factor, inch/minute);
  }
  static boost::units::quantity<boost::units::si::velocity> get_unit(const std::string& s) {
    // It's either "length/time" or "length per time".
    std::string argument(s);
    std::string numerator;
    std::string denominator;
    try {
      numerator = get_word(argument);
      get_division(argument);
      denominator = get_word(argument);
    } catch (parse_exception e) {
      printf("throwing an error\n");
      std::cout << e.what() << std::endl;
      throw boost::program_options::validation_error(
          boost::program_options::validation_error::invalid_option_value,
          argument);
    }
    return Length::get_unit(numerator)/Time::get_unit(denominator);
  }
};

template<>
class Unit<boost::units::si::frequency> : public UnitBase<boost::units::si::frequency> {
 public:
  Unit(double value, boost::optional<boost::units::quantity<boost::units::si::frequency>> one) : UnitBase(value, one) {}
  double asPerMinute(double factor) const {
    return as(factor, 1.0/minute);
  }
  static boost::units::quantity<boost::units::si::frequency> get_unit(const std::string& s) {
    // It's either "dimensionless/time" or "dimensionless per time".
    boost::match_results<const char*> m;
    if (!regex_match(s.c_str(), m, boost::regex("\\s*(\\S*)\\s*(?:/|\\s[pP][eE][rR]\\s)\\s*(\\S*)\\s*"))) {
      boost::program_options::validation_error(
          boost::program_options::validation_error::invalid_option_value);
    }
    const std::string numerator(m[1].first, m[1].second);
    const std::string denominator(m[2].first, m[2].second);
    return Dimensionless::get_unit(numerator)/Time::get_unit(denominator);
  }
};

template<typename dimension_t>
void validate(boost::any& v,
              const std::vector<std::string>& values,
              Unit<dimension_t>*, int) {
  // Make sure no previous assignment was made.
  boost::program_options::validators::check_first_occurrence(v);
  // Extract the first string from 'values'. If there is more than
  // one string, it's an error, and exception will be thrown.
  const std::string& s = boost::program_options::validators::get_single_string(values);

  // Figure out what unit it is.
  std::string argument(s); // Make a copy.
  double value;
  boost::optional<boost::units::quantity<dimension_t>> one = boost::none;
  printf("trying to parse %s\n", argument.c_str());
  try {
    value = get_double(argument);
    printf("got value %f\n", value);
    printf("argument is now %s\n", argument.c_str());
    if (argument.size() > 0) {
      one = Unit<dimension_t>::get_unit(argument);
    }
  } catch (parse_exception e) {
    printf("throwing an error\n");
    std::cout << e.what() << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value,
        argument);
  }
  get_whitespace(argument);
  if (argument.size() > 0) {
    printf("leftover in argument is: %s\n", argument.c_str());
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value,
        argument);
  }
  v = boost::any(Unit<dimension_t>(value, one));
}

} // namespace

#endif // UNITS_HPP
