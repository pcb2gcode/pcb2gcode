#ifndef AVAILABLE_DRILLS_HPP
#define AVAILABLE_DRILLS_HPP

#include <boost/program_options.hpp>                         // for invalid_option_value, program_options
#include <boost/units/unit.hpp>                              // for operator>
#include <math.h>                                            // for abs
#include <stdlib.h>                                          // for abs
#include <iostream>                                          // for operator<<, ostream, istream, basic_ostream, endl, cerr
#include <iterator>                                          // for istreambuf_iterator, operator!=, operator==
#include <limits>                                            // for numeric_limits
#include <string>                                            // for string, operator+, char_traits
#include <utility>                                           // for swap
#include <vector>                                            // for vector

#include "boost/algorithm/string/classification.hpp"         // for is_any_of
#include "boost/algorithm/string/detail/classification.hpp"  // for is_any_ofF
#include "boost/algorithm/string/split.hpp"                  // for split
#include "boost/iterator/iterator_facade.hpp"                // for operator!=
#include "boost/none.hpp"                                    // for none
#include "boost/optional/optional.hpp"                       // for optional
#include "boost/type_index/type_index_facade.hpp"            // for operator==
#include "units.hpp"                                         // for Length, units_parse_exception, operator<<, parse_unit, UnitBase, CommaSeparated, Unit

namespace po = boost::program_options;

class AvailableDrill {
 public:
  AvailableDrill(Length diameter = Length(0),
                 Length negative_tolerance = Length(-std::numeric_limits<double>::infinity()),
                 Length positive_tolerance = Length(std::numeric_limits<double>::infinity())) :
      diameter_(diameter), negative_tolerance(negative_tolerance), positive_tolerance(positive_tolerance) {}
  friend inline std::istream& operator>>(std::istream& in, AvailableDrill& available_drill);
  bool operator==(const AvailableDrill& other) const {
    return diameter_ == other.diameter_ &&
        negative_tolerance == other.negative_tolerance &&
        positive_tolerance == other.positive_tolerance;
  }
  Length diameter() const {
    return diameter_;
  }
  std::ostream& write(std::ostream& out) const {
    out << diameter_;
    if (negative_tolerance == Length(-std::numeric_limits<double>::infinity()) &&
        positive_tolerance == Length( std::numeric_limits<double>::infinity())) {
      return out;
    }
    out << ":" << negative_tolerance << ":+" << positive_tolerance;
    return out;
  }
  void read(const std::string& input_string) {
    std::vector<std::string> drill_parts;
    boost::split(drill_parts, input_string, boost::is_any_of(":"));
    switch (drill_parts.size()) {
      case 3:
        positive_tolerance = parse_unit<Length>(drill_parts[2]);
        // no break
      case 2:
        negative_tolerance = parse_unit<Length>(drill_parts[1]);
        // no break
      case 1:
        diameter_ = parse_unit<Length>(drill_parts[0]);
        break;
      default:
        throw units_parse_exception("Too many parts in " + input_string);
    }
    if (drill_parts.size() == 2) {
      positive_tolerance = -negative_tolerance;
    }
    if (positive_tolerance.asInch(1) < 0 || negative_tolerance.asInch(1) > 0) {
      std::swap(positive_tolerance, negative_tolerance);
    }
    if (positive_tolerance.asInch(1) < 0 || negative_tolerance.asInch(1) > 0) {
      throw units_parse_exception("One tolerance must be negative and "
                                  "one must be positive");
    }
  }
  // Returns the difference in inches.
  boost::optional<double> difference(Length wanted_diameter, double inputFactor) const {
    if (wanted_diameter.asInch(inputFactor) >= diameter_.asInch(inputFactor) + negative_tolerance.asInch(inputFactor) &&
        wanted_diameter.asInch(inputFactor) <= diameter_.asInch(inputFactor) + positive_tolerance.asInch(inputFactor)) {
      return std::abs(wanted_diameter.asInch(inputFactor) - diameter_.asInch(inputFactor));
    } else {
      return boost::none;
    }
  }
 private:
  // The diameter for holes that this drill can be used to drill.
  Length diameter_;
  // Tolerance for drilling holes smaller than the diameter.  This
  // number must be non-positive.
  Length negative_tolerance;
  // Tolerance for drilling holes larger than the diameter.  This
  // number must be non-negative.
  Length positive_tolerance;
};

inline std::istream& operator>>(std::istream& in, AvailableDrill& available_drill) {
  std::string input_string(std::istreambuf_iterator<char>(in), {});
  try {
    available_drill.read(input_string);
  } catch (units_parse_exception& e) {
    std::cerr << e.what() << std::endl;
    throw boost::program_options::invalid_option_value(input_string);
  }
  return in;
}

inline std::ostream& operator<<(std::ostream& out, const AvailableDrill& available_drill) {
  return available_drill.write(out);
}

typedef CommaSeparated<AvailableDrill> AvailableDrills;

#endif // AVAILABLE_DRILLS_HPP
