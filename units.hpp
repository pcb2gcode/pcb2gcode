#ifndef UNITS_HPP
#define UNITS_HPP

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/units/quantity.hpp>
#include <boost/optional.hpp>
#include <boost/units/systems/si.hpp>
#include <boost/units/base_units/metric/minute.hpp>
#include <boost/units/base_units/imperial/inch.hpp>

namespace {

// String parsers: Each on uses characters from the front of the
// string and leaves the unused characters in place.
struct parse_exception : public std::exception {
  parse_exception(const std::string& get_what, const std::istringstream& from_what) {
    std::ostringstream what_stream;
    what_stream << "Can't get " << get_what << " from: " << from_what.rdbuf();
    what_string = what_stream.str();
  }

  virtual const char* what() const throw()
  {
    return what_string.c_str();
  }
 private:
  std::string what_string;
};

void get_whitespace(std::istringstream& input) {
  input >> std::ws;
}

// Gets the double from the start of the string which may start with
// whitespace.  Throws an exception if it fails.
double get_double(std::istringstream& input) {
  get_whitespace(input);
  std::streampos pos = input.tellg();
  double result;
  if (input >> result) {
    return result;
  } else {
    input.seekg(pos, input.beg);
    throw parse_exception("double", input);
  }
}

// Get the word from the start of the string which may have spaces.
// All characters up to the first that is not std::isalnum are
// returned.  They are removed from the input.  The result might have
// length 0.
std::string get_word(std::istringstream& input) {
  get_whitespace(input);
  std::string result;
  while (std::isalnum(input.peek())) {
    result += input.get();
  }
  return result;
}

// Get division indicator from the start of the string, either a slash
// or "per", which must have no spaces.  The division character is
// removed from the input.  Throws if it can't find the division
// character.
void get_division(std::istringstream& input) {
  get_whitespace(input);
  std::streampos pos = input.tellg();
  switch (input.get()) {
    case '/':
      return;
    case 'p':
      if (input.get() == 'e' && input.get() == 'r') {
        return;
      }
  }
  input.seekg(pos, input.beg);
  throw parse_exception("division symbol", input);
}

template <typename dimension_t>
class UnitBase {
 public:
  typedef boost::units::quantity<dimension_t> quantity;
  UnitBase(double value, boost::optional<quantity> one) : value(value), one(one) {}
  double as(double factor, quantity wanted_unit) const {
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
  boost::optional<quantity> one;
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
  Unit(double value, boost::optional<quantity> one) : UnitBase(value, one) {}
  double asInch(double factor) const {
    return as(factor, inch);
  }
  static quantity get_unit(std::istringstream& stream) {
    std::streampos pos = stream.tellg();
    std::string unit = get_word(stream);
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
    stream.seekg(pos, stream.beg);
    throw parse_exception("length", stream);
  }
};

template<>
class Unit<boost::units::si::time> : public UnitBase<boost::units::si::time> {
 public:
  Unit(double value, boost::optional<quantity> one) : UnitBase(value, one) {}
  double asSecond(double factor) const {
    return as(factor, 1.0*boost::units::si::second);
  }
  static quantity get_unit(std::istringstream& stream) {
    std::streampos pos = stream.tellg();
    std::string unit = get_word(stream);
    if (unit == "s" ||
        unit == "second" ||
        unit == "seconds") {
      return 1.0*boost::units::si::second;
    }
    if (unit == "min" ||
        unit == "mins" ||
        unit == "minute" ||
        unit == "minutes") {
      return minute;
    }
    stream.seekg(pos, stream.beg);
    throw parse_exception("time", stream);
  }
};

template<>
class Unit<boost::units::si::dimensionless> : public UnitBase<boost::units::si::dimensionless> {
 public:
  Unit(double value, boost::optional<quantity> one) : UnitBase(value, one) {}
  using UnitBase::as;
  double as(double factor) const {
    return as(factor, 1.0*boost::units::si::si_dimensionless);
  }
  static quantity get_unit(std::istringstream& stream) {
    std::streampos pos = stream.tellg();
    std::string unit = get_word(stream);
    if (unit == "rotation" ||
        unit == "rotations" ||
        unit == "cycle" ||
        unit == "cycles") {
      return 1.0*boost::units::si::si_dimensionless;
    }
    stream.seekg(pos, stream.beg);
    throw parse_exception("dimensionless", stream);
  }
};

template<>
class Unit<boost::units::si::velocity> : public UnitBase<boost::units::si::velocity> {
 public:
  Unit(double value, boost::optional<quantity> one) : UnitBase(value, one) {}
  double asInchPerMinute(double factor) const {
    return as(factor, inch/minute);
  }
  static quantity get_unit(std::istringstream& stream) {
    // It's either "length/time" or "length per time".
    Length::quantity numerator;
    Time::quantity denominator;
    numerator = Length::get_unit(stream);
    get_division(stream);
    denominator = Time::get_unit(stream);
    return numerator/denominator;
  }
};

template<>
class Unit<boost::units::si::frequency> : public UnitBase<boost::units::si::frequency> {
 public:
  Unit(double value, boost::optional<quantity> one) : UnitBase(value, one) {}
  double asPerMinute(double factor) const {
    return as(factor, 1.0/minute);
  }
  static quantity get_unit(std::istringstream& stream) {
    // It's either "dimensionless/time" or "dimensionless per time".
    Dimensionless::quantity numerator;
    Time::quantity denominator;
    numerator = Dimensionless::get_unit(stream);
    get_division(stream);
    denominator = Time::get_unit(stream);
    return numerator/denominator;
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
  std::istringstream stream(s); // Make a copy.
  double value;
  boost::optional<boost::units::quantity<dimension_t>> one = boost::none;
  try {
    value = get_double(stream);
    get_whitespace(stream);
    if (!stream.eof()) {
      one = Unit<dimension_t>::get_unit(stream);
    }
  } catch (parse_exception& e) {
    std::cerr << e.what() << std::endl;
    throw boost::program_options::invalid_option_value(stream.str());
  }
  get_whitespace(stream);
  if (!stream.eof()) {
    throw boost::program_options::invalid_option_value(stream.str());
  }
  v = boost::any(Unit<dimension_t>(value, one));
}

} // namespace

#endif // UNITS_HPP
