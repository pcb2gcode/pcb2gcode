#include "output_manifest.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

namespace output_manifest {

using json = nlohmann::ordered_json;
using std::string;
using std::vector;
namespace fs = std::filesystem;

namespace {

// Bump this when a change could break a reader of the manifest.  Adding keys
// doesn't count.
constexpr int format_version = 1;

std::mutex entries_mutex;
// The path to each file and the rest of its entry.
vector<std::pair<string, json>> entries;

void add(const string& path, json entry) {
  std::lock_guard<std::mutex> lock(entries_mutex);
  entries.emplace_back(path, std::move(entry));
}

// Rounded to the nanometre so that 0.2mm doesn't come out as
// 0.19999999999999998mm after the round trip through inches.
double inch_to_mm(double inches) {
  return std::round(inches * 25.4 * 1e6) / 1e6;
}

// generic_u8string() returns std::u8string from C++20 on.
string utf8(const fs::path& path) {
  const auto s = path.generic_u8string();
  return string(s.cbegin(), s.cend());
}

// Relative to manifest_dir if the file is inside it, otherwise absolute.
string path_in_manifest(const string& path, const fs::path& manifest_dir) {
  const fs::path absolute = fs::absolute(path).lexically_normal();
  const fs::path relative = absolute.lexically_relative(manifest_dir);
  if (relative.empty() || *relative.begin() == "..") {
    return utf8(absolute);
  }
  return utf8(relative);
}

} // namespace

void add_gcode(const string& path, const string& layer, const vector<Tool>& tools) {
  json tools_json = json::array();
  for (const auto& tool : tools) {
    tools_json.push_back({{"number", tool.number},
                          {"diameter_mm", inch_to_mm(tool.diameter)}});
  }
  add(path, {{"type", "gcode"}, {"layer", layer}, {"tools", tools_json}});
}

void add_svg(const string& path, const string& layer, const string& stage) {
  add(path, {{"type", "svg"}, {"layer", layer}, {"stage", stage}});
}

void add_svg(const string& path, const string& layer, const string& stage,
             bool is_final, size_t tool_index, double tool_diameter) {
  add(path, {{"type", "svg"}, {"layer", layer}, {"stage", stage},
             {"final", is_final}, {"tool_index", tool_index},
             {"tool_diameter_mm", inch_to_mm(tool_diameter)}});
}

string to_json(const string& manifest_path) {
  const fs::path manifest_dir =
      fs::absolute(manifest_path).lexically_normal().parent_path();
  vector<json> files;
  {
    std::lock_guard<std::mutex> lock(entries_mutex);
    for (const auto& path_and_entry : entries) {
      json file = {{"path", path_in_manifest(path_and_entry.first, manifest_dir)}};
      file.update(path_and_entry.second);
      files.push_back(std::move(file));
    }
  }
  // Layers are exported in parallel, so sort for a repeatable order.
  std::sort(files.begin(), files.end(), [](const json& a, const json& b) {
    if (a.at("path") != b.at("path")) {
      return a.at("path") < b.at("path");
    }
    return a < b;
  });
  const json manifest = {{"format_version", format_version}, {"files", files}};
  return manifest.dump(2, ' ', false, json::error_handler_t::replace) + "\n";
}

void write(const string& manifest_path) {
  std::ofstream out(manifest_path);
  if (!out.is_open()) {
    throw std::invalid_argument("Can't open for writing: " + manifest_path);
  }
  out << to_json(manifest_path);
}

void clear() {
  std::lock_guard<std::mutex> lock(entries_mutex);
  entries.clear();
}

} // namespace output_manifest
