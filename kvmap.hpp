#ifndef KVMAP_HPP
#define KVMAP_HPP

#include <unordered_map>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>

#include "units.hpp"

namespace po = boost::program_options;

class KVMap : public std::unordered_map<std::string, std::string> {
 public:
  friend inline std::istream& operator>>(std::istream& in, KVMap& kvmap);

  std::ostream& write(std::ostream& out) const {
    if (size() == 0) {
      return out;
    }
    auto kv = cbegin();
    while (true) {
      out << kv->first << "=" << kv->second;
      kv++;
      if (kv == cend()) {
        return out;
      }
      out << ":";
    }
  }
  void read(const std::string& input_string) {
    std::vector<std::string> kv_parts;
    boost::split(kv_parts, input_string, boost::is_any_of(":"));
    for (const auto& kv : kv_parts) {
      std::vector<std::string> kv_pair;
      boost::split(kv_pair, kv, boost::is_any_of("="));
      if (kv_pair.size() != 2) {
        throw boost::program_options::invalid_option_value(kv);
      }
      emplace(kv_pair[0], kv_pair[1]);
    }
  }
};

inline std::istream& operator>>(std::istream& in, KVMap& kvmap) {
  std::string input_string(std::istreambuf_iterator<char>(in), {});
  kvmap.read(input_string);
  return in;
}

inline std::ostream& operator<<(std::ostream& out, const KVMap& kvmap) {
  return kvmap.write(out);
}

typedef CommaSeparated<KVMap> KVMaps;

#endif // KVMAP_HPP
