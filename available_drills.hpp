#ifndef AVAILABLE_DRILLS_HPP
#define AVAILABLE_DRILLS_HPP

#include "units.hpp"
#include <boost/program_options.hpp>
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
  void read(const string& input_string) {
    std::vector<string> drill_parts;
    boost::split(drill_parts, input_string, boost::is_any_of(":"));
    switch (drill_parts.size()) {
      case 3:
        positive_tolerance = ::parse_unit<Length>(drill_parts[2]);
        // no break
      case 2:
        negative_tolerance = ::parse_unit<Length>(drill_parts[1]);
        // no break
      case 1:
        diameter_ = ::parse_unit<Length>(drill_parts[0]);
        break;
      default:
        throw parse_exception("Too many parts in " + input_string);
    }
    if (drill_parts.size() == 2) {
      positive_tolerance = -negative_tolerance;
    }
    if (positive_tolerance.asInch(1) < 0 || negative_tolerance.asInch(1) > 0) {
      std::swap(positive_tolerance, negative_tolerance);
    }
    if (positive_tolerance.asInch(1) < 0 || negative_tolerance.asInch(1) > 0) {
      throw parse_exception("One tolerance must be negative and "
                            "one must be positive");
    }
  }
  // Returns the difference in inches.
  boost::optional<double> difference(Length wanted_diameter, double inputFactor) const {
    if (wanted_diameter.asInch(inputFactor) >= diameter_.asInch(inputFactor) + negative_tolerance.asInch(inputFactor) &&
        wanted_diameter.asInch(inputFactor) <= diameter_.asInch(inputFactor) + positive_tolerance.asInch(inputFactor)) {
      return abs(wanted_diameter.asInch(inputFactor) - diameter_.asInch(inputFactor));
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
  } catch (parse_exception& e) {
    std::cerr << e.what() << std::endl;
    throw boost::program_options::invalid_option_value(input_string);
  }
  return in;
}

inline std::ostream& operator<<(std::ostream& out, const AvailableDrill& available_drill) {
  return available_drill.write(out);
}

class AvailableDrills {
 public:
  AvailableDrills(const std::vector<AvailableDrill>& available_drills) :
      available_drills(available_drills) {}
  AvailableDrills() {}
  friend inline std::istream& operator>>(std::istream& in, AvailableDrills& available_drills);
  bool operator==(const AvailableDrills& other) const {
    return available_drills == other.available_drills;
  }
  const std::vector<AvailableDrill>& get_available_drills() const {
    return available_drills;
  }
  std::ostream& write(std::ostream& out) const {
    for (auto it = available_drills.begin(); it != available_drills.end(); it++) {
      if (it != available_drills.begin()) {
        out << ", ";
      }
      out << *it;
    }
    return out;
  }
  std::istream& read(std::istream& in) {
    std::vector<string> available_drill_strings;
    std::string input_string(std::istreambuf_iterator<char>(in), {});
    boost::split(available_drill_strings, input_string, boost::is_any_of(","));
    for (const auto& available_drill_string : available_drill_strings) {
      AvailableDrill available_drill;
      std::stringstream(available_drill_string) >> available_drill;
      available_drills.push_back(available_drill);
    }
    return in;
  }
 private:
  std::vector<AvailableDrill> available_drills;
};

inline std::istream& operator>>(std::istream& in, AvailableDrills& available_drills) {
  return available_drills.read(in);
}

inline std::ostream& operator<<(std::ostream& out, const AvailableDrills& available_drills) {
  return available_drills.write(out);
}

#endif // AVAILABLE_DRILLS_HPP
