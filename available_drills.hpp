#ifndef AVAILABLE_DRILLS_HPP
#define AVAILABLE_DRILLS_HPP

class AvailableDrill {
 public:
  AvailableDrill(Length diameter) : diameter_(diameter) {}
  AvailableDrill() {}
  friend inline std::istream& operator>>(std::istream& in, AvailableDrill& available_drill);
  bool operator==(const AvailableDrill& other) const {
    return diameter_ == other.diameter_;
  }
  Length diameter() const {
    return diameter_;
  }
  std::ostream& write(std::ostream& out) const {
    out << diameter_;
    return out;
  }
  std::istream& read(std::istream& in) {
    in >> diameter_;
    return in;
  }
  // Returns the difference in inches.
  double difference(Length wanted_diameter, double inputFactor) {
    return abs(wanted_diameter.asInch(inputFactor) -
               diameter_.asInch(inputFactor));
  }
 private:
  Length diameter_;
};

inline std::istream& operator>>(std::istream& in, AvailableDrill& available_drill) {
  return available_drill.read(in);
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
