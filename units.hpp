#ifndef UNITS_HPP
#define UNITS_HPP

#include <iostream>
#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>
#include <boost/units/quantity.hpp>
#include <boost/optional.hpp>
#include <boost/units/systems/si/length.hpp>
#include <boost/units/systems/si/velocity.hpp>
#include <boost/units/base_units/imperial/inch.hpp>

// dimension_t is "length" or "velocity", for example.
template<typename dimension_t> class Unit;

template<>
class Unit<boost::units::si::length> {
 public:
  Unit(double value, boost::optional<boost::units::quantity<boost::units::si::length>> one) : value(value), one(one) {}
  double asDouble() const {
    return value;
  }
  double asInch(double factor) const {
    if (!one) {
      // We don't know the units so just use whatever factor was supplied.
      return value*factor;
    }
    return value*(*one)/(boost::units::conversion_factor(boost::units::imperial::inch_base_unit::unit_type(),
                                                         boost::units::si::meter) * boost::units::si::meter);
  }
  static boost::units::quantity<boost::units::si::length> get_unit(const std::string& s) {
    if (s == "mm" ||
        s == "millimeter" ||
        s == "millimeters") {
      return boost::units::si::meter/1000.0;
    }
    if (s == "inch" ||
        s == "inches") {
      return boost::units::conversion_factor(boost::units::imperial::inch_base_unit::unit_type(),
                                             boost::units::si::meter) * boost::units::si::meter;
    }
    std::cerr << "Don't recognize units: " << s << std::endl;
    throw boost::program_options::validation_error(
        boost::program_options::validation_error::invalid_option_value);
  }

 private:
  double value;
  boost::optional<boost::units::quantity<boost::units::si::length>> one;
};

typedef Unit<boost::units::si::length> Length;
typedef Unit<boost::units::si::velocity> Velocity;

// This calls the functions that actual do the work that are wrapped
// in a struct so that we can template on the return type.
template<typename dimension_t> typename boost::units::quantity<dimension_t> get_unit(const std::string& s) {
  return Unit<dimension_t>::get_unit(s);
}

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
    if (!regex_match(s.c_str(), m, boost::regex("\\s*([0-9.]+)\\s*(\\S*)\\s*"))) {
      boost::program_options::validation_error(
          boost::program_options::validation_error::invalid_option_value);
    }
    double value = boost::lexical_cast<double>(std::string(m[1].first, m[1].second));
    boost::optional<boost::units::quantity<dimension_t>> one = boost::none;
    if (m[2].length() > 0) {
      one = Unit<boost::units::si::length>::get_unit(std::string(m[2].first, m[2].second).c_str());
    }
    v = boost::any(Unit<dimension_t>(value, one));
}

#endif // UNITS_HPP
