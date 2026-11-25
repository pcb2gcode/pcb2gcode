#define BOOST_TEST_MODULE output_manifest tests
#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "output_manifest.hpp"

using nlohmann::json;
namespace fs = std::filesystem;

namespace {

json manifest(const std::string& manifest_path = "manifest.json") {
  return json::parse(output_manifest::to_json(manifest_path));
}

std::string absolute(const std::string& path) {
  return (fs::current_path() / path).lexically_normal().generic_string();
}

struct EmptyManifest {
  EmptyManifest() { output_manifest::clear(); }
  ~EmptyManifest() { output_manifest::clear(); }
};

} // namespace

BOOST_FIXTURE_TEST_SUITE(output_manifest_tests, EmptyManifest)

BOOST_AUTO_TEST_CASE(empty) {
  BOOST_CHECK_EQUAL(manifest(), json::parse(R"({"format_version": 1, "files": []})"));
}

BOOST_AUTO_TEST_CASE(gcode) {
  output_manifest::add_gcode("front.ngc", "front", {{1, 0.2 / 25.4}, {3, 0.5}});
  BOOST_CHECK_EQUAL(manifest()["files"], json::parse(R"([
      {"path": "front.ngc", "type": "gcode", "layer": "front",
       "tools": [{"number": 1, "diameter_mm": 0.2}, {"number": 3, "diameter_mm": 12.7}]}])"));
}

BOOST_AUTO_TEST_CASE(svgs) {
  output_manifest::add_svg("outp0_original_back.svg", "back", "original");
  output_manifest::add_svg("processed_back_final_1.svg", "back", "processed", true, 1, 0.8 / 25.4);
  BOOST_CHECK_EQUAL(manifest()["files"], json::parse(R"([
      {"path": "outp0_original_back.svg", "type": "svg", "layer": "back", "stage": "original"},
      {"path": "processed_back_final_1.svg", "type": "svg", "layer": "back", "stage": "processed",
       "final": true, "tool_index": 1, "tool_diameter_mm": 0.8}])"));
}

BOOST_AUTO_TEST_CASE(diameters_rounded_to_nanometres) {
  output_manifest::add_gcode("front.ngc", "front", {{1, 1.0 / 3}});
  BOOST_CHECK_EQUAL(manifest()["files"][0]["tools"][0]["diameter_mm"], 8.466667);
}

BOOST_AUTO_TEST_CASE(sorted_by_path) {
  output_manifest::add_svg("traced_front.svg", "front", "traced", false, 0, 0.01);
  output_manifest::add_gcode("front.ngc", "front", {});
  output_manifest::add_svg("outp0_original_front.svg", "front", "original");
  const json files = manifest()["files"];
  std::vector<std::string> paths;
  for (const auto& file : files) {
    paths.push_back(file["path"]);
  }
  const std::vector<std::string> expected{"front.ngc", "outp0_original_front.svg", "traced_front.svg"};
  BOOST_CHECK_EQUAL_COLLECTIONS(paths.cbegin(), paths.cend(), expected.cbegin(), expected.cend());
}

BOOST_AUTO_TEST_CASE(same_path_in_any_order) {
  output_manifest::add_gcode("board.ngc", "front", {});
  output_manifest::add_gcode("board.ngc", "back", {});
  const std::string front_first = output_manifest::to_json("manifest.json");
  output_manifest::clear();
  output_manifest::add_gcode("board.ngc", "back", {});
  output_manifest::add_gcode("board.ngc", "front", {});
  BOOST_CHECK_EQUAL(output_manifest::to_json("manifest.json"), front_first);
}

BOOST_AUTO_TEST_CASE(paths_relative_to_manifest) {
  output_manifest::add_gcode("out/front.ngc", "front", {});
  output_manifest::add_svg("out/images/traced_front.svg", "front", "traced", false, 0, 0.01);
  output_manifest::add_svg("out/images/../outp0_original_front.svg", "front", "original");
  output_manifest::add_gcode("back.ngc", "back", {});
  output_manifest::add_gcode(absolute("elsewhere/drill.ngc"), "drill", {});
  const json files = manifest("out/manifest.json")["files"];
  std::set<std::string> paths;
  for (const auto& file : files) {
    paths.insert(file["path"].get<std::string>());
  }
  const std::set<std::string> expected{
    "front.ngc", "images/traced_front.svg", "outp0_original_front.svg",
    absolute("back.ngc"), absolute("elsewhere/drill.ngc")};
  BOOST_CHECK_EQUAL_COLLECTIONS(paths.cbegin(), paths.cend(), expected.cbegin(), expected.cend());
}

BOOST_AUTO_TEST_CASE(absolute_manifest_path) {
  output_manifest::add_gcode("out/front.ngc", "front", {});
  BOOST_CHECK_EQUAL(manifest(absolute("out/manifest.json"))["files"][0]["path"], "front.ngc");
}

BOOST_AUTO_TEST_CASE(add_from_threads) {
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; t++) {
    threads.emplace_back([t]() {
      for (int i = 0; i < 100; i++) {
        output_manifest::add_svg(std::to_string(t) + "_" + std::to_string(i) + ".svg",
                                 "front", "original");
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
  BOOST_CHECK_EQUAL(manifest()["files"].size(), 400u);
}

BOOST_AUTO_TEST_CASE(write) {
  const fs::path dir = fs::temp_directory_path() / "pcb2gcode_output_manifest_tests";
  fs::create_directories(dir);
  const std::string manifest_path = (dir / "manifest.json").string();
  output_manifest::add_gcode((dir / "front.ngc").string(), "front", {{1, 0.01}});
  output_manifest::write(manifest_path);
  std::ifstream written(manifest_path);
  std::stringstream contents;
  contents << written.rdbuf();
  written.close();
  fs::remove_all(dir);
  BOOST_CHECK_EQUAL(contents.str(), output_manifest::to_json(manifest_path));
  BOOST_CHECK_EQUAL(json::parse(contents.str())["files"][0]["path"], "front.ngc");
}

BOOST_AUTO_TEST_CASE(write_to_missing_directory) {
  BOOST_CHECK_THROW(output_manifest::write("no/such/directory/manifest.json"),
                    std::invalid_argument);
}

BOOST_AUTO_TEST_SUITE_END()
