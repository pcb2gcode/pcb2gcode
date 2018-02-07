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
    if (s == "mm" ||
        s == "millimeter" ||
        s == "millimeters") {
      return boost::units::si::meter/1000.0;
    }
    if (s == "in" ||
        s == "inch" ||
        s == "inches") {
      return inch;
    }
    std::cerr << "Didn't recognize units of length: " << s << std::endl;
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
  double asSecond(double factor) const {
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
    boost::match_results<const char*> m;
    if (!regex_match(s.c_str(), m, boost::regex("\\s*(\\S*)\\s*(?:/|\\s[pP][eE][rR]\\s)\\s*(\\S*)\\s*"))) {
      boost::program_options::validation_error(
          boost::program_options::validation_error::invalid_option_value);
    }
    const std::string numerator(m[1].first, m[1].second);
    const std::string denominator(m[2].first, m[2].second);
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
              Unit<dimension_t>*, int)
{
    // Make sure no previous assignment was made.
    boost::program_options::validators::check_first_occurrence(v);
    // Extract the first string from 'values'. If there is more than
    // one string, it's an error, and exception will be thrown.
    const std::string& s = boost::program_options::validators::get_single_string(values);

    // Figure out what unit it is.
    boost::match_results<const char*> m;
    if (!regex_match(s.c_str(), m, boost::regex("\\s*([-0-9.]+)\\s*(.*?)\\s*"))) {
      boost::program_options::validation_error(
          boost::program_options::validation_error::invalid_option_value);
    }
    std::string value_string(m[1].first, m[1].second);
    double value;
    try {
      value = boost::lexical_cast<double>(value_string);
    } catch (std::exception& e) {
      std::cerr << "Error parsing as number: " << value_string << std::endl;
      throw;
    }
    boost::optional<boost::units::quantity<dimension_t>> one = boost::none;
    if (m[2].length() > 0) {
      one = Unit<dimension_t>::get_unit(std::string(m[2].first, m[2].second).c_str());
    }
    v = boost::any(Unit<dimension_t>(value, one));
}

#endif // UNITS_HPP
