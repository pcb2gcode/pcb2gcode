#ifndef UNITS_HPP
#define UNITS_HPP

#include <string>
#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/regex.hpp>

template<typename dimension_t>
class Unit {
 public:
  Unit(std::string value) : value(value) {}
  std::string value;
  double asDouble() const {
    return boost::lexical_cast<double>(value.data(),1);
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
    if (!regex_search(s.c_str(), m, boost::regex("^\\s*[0-9.]\\s*([^ ]+)?"))) {
      throw boost::program_options::validation_error(
          boost::program_options::validation_error::invalid_option_value);
    }
    //boost::units::quantity<dimension_t> result(1.0 * boost::units::si::meter);
    v = boost::any(Unit<dimension_t>(s));
}


#endif // UNITS_HPP
