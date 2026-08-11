#define BOOST_TEST_MODULE mcp_server tests
#include <boost/test/unit_test.hpp>

#include "mcp_server.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>
#include <string>

using json = nlohmann::json;

#ifndef PCB2GCODE_TEST_BINARY
#error "PCB2GCODE_TEST_BINARY must be defined at build time (see tests/CMakeLists.txt)"
#endif

namespace {

// Redirect cin from `input` and cout into a fresh stringstream, run the MCP
// server against `binary` until stdin is exhausted, restore streams, and
// return the captured stdout. Keeps the fixture ceremony out of each test.
std::string run_mcp_with_input(const std::string& input,
                               const char* binary = "pcb2gcode") {
  std::streambuf* orig_cin = std::cin.rdbuf();
  std::streambuf* orig_cout = std::cout.rdbuf();

  std::istringstream iss(input);
  std::ostringstream oss;
  std::cin.rdbuf(iss.rdbuf());
  std::cout.rdbuf(oss.rdbuf());

  run_mcp_server(binary);

  std::cin.rdbuf(orig_cin);
  std::cout.rdbuf(orig_cout);

  return oss.str();
}

}  // namespace

BOOST_AUTO_TEST_SUITE(mcp_server_tests)

BOOST_AUTO_TEST_CASE(mcp_server_initialize_test) {
  std::streambuf* orig_cin = std::cin.rdbuf();
  std::streambuf* orig_cout = std::cout.rdbuf();

  std::string input = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}\n";
  std::istringstream iss(input);
  std::ostringstream oss;

  std::cin.rdbuf(iss.rdbuf());
  std::cout.rdbuf(oss.rdbuf());

  run_mcp_server("pcb2gcode");

  std::cin.rdbuf(orig_cin);
  std::cout.rdbuf(orig_cout);

  std::string output = oss.str();
  BOOST_CHECK(!output.empty());

  json resp = json::parse(output);
  BOOST_CHECK_EQUAL(resp["jsonrpc"], "2.0");
  BOOST_CHECK_EQUAL(resp["id"], 1);
  BOOST_CHECK_EQUAL(resp["result"]["serverInfo"]["name"], "pcb2gcode");
}

BOOST_AUTO_TEST_CASE(mcp_server_ping_test) {
  std::streambuf* orig_cin = std::cin.rdbuf();
  std::streambuf* orig_cout = std::cout.rdbuf();

  std::string input = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"ping\",\"params\":{}}\n";
  std::istringstream iss(input);
  std::ostringstream oss;

  std::cin.rdbuf(iss.rdbuf());
  std::cout.rdbuf(oss.rdbuf());

  run_mcp_server("pcb2gcode");

  std::cin.rdbuf(orig_cin);
  std::cout.rdbuf(orig_cout);

  std::string output = oss.str();
  BOOST_CHECK(!output.empty());

  json resp = json::parse(output);
  BOOST_CHECK_EQUAL(resp["jsonrpc"], "2.0");
  BOOST_CHECK_EQUAL(resp["id"], 2);
}

BOOST_AUTO_TEST_CASE(mcp_server_tools_list_test) {
  std::streambuf* orig_cin = std::cin.rdbuf();
  std::streambuf* orig_cout = std::cout.rdbuf();

  std::string input = "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/list\",\"params\":{}}\n";
  std::istringstream iss(input);
  std::ostringstream oss;

  std::cin.rdbuf(iss.rdbuf());
  std::cout.rdbuf(oss.rdbuf());

  run_mcp_server("pcb2gcode");

  std::cin.rdbuf(orig_cin);
  std::cout.rdbuf(orig_cout);

  std::string output = oss.str();
  BOOST_CHECK(!output.empty());

  json resp = json::parse(output);
  BOOST_CHECK_EQUAL(resp["jsonrpc"], "2.0");
  BOOST_CHECK_EQUAL(resp["id"], 3);
  BOOST_CHECK(resp["result"].contains("tools"));
  BOOST_CHECK_EQUAL(resp["result"]["tools"].size(), 3);
}

// tools/call tests: these exercise the fork/exec path inside run_mcp_server,
// so they need a real pcb2gcode binary — the built one, injected at compile
// time via PCB2GCODE_TEST_BINARY. Each test hits a distinct branch of the
// tool_name dispatch in mcp_server.cpp.

BOOST_AUTO_TEST_CASE(mcp_server_tools_call_version_test) {
  std::string input =
      "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"tools/call\","
      "\"params\":{\"name\":\"pcb2gcode_version\",\"arguments\":{}}}\n";
  json resp = json::parse(run_mcp_with_input(input, PCB2GCODE_TEST_BINARY));
  BOOST_CHECK_EQUAL(resp["id"], 10);
  BOOST_CHECK_EQUAL(resp["result"]["isError"], false);
  std::string text = resp["result"]["content"][0]["text"].get<std::string>();
  BOOST_CHECK_NE(text.find("Exit code: 0"), std::string::npos);
  // `--version` prints a "Git commit:" metadata line; use it as a sentinel
  // that we really invoked --version rather than something else.
  BOOST_CHECK_NE(text.find("Git commit:"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(mcp_server_tools_call_help_test) {
  std::string input =
      "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"tools/call\","
      "\"params\":{\"name\":\"pcb2gcode_help\",\"arguments\":{}}}\n";
  json resp = json::parse(run_mcp_with_input(input, PCB2GCODE_TEST_BINARY));
  BOOST_CHECK_EQUAL(resp["id"], 11);
  BOOST_CHECK_EQUAL(resp["result"]["isError"], false);
  std::string text = resp["result"]["content"][0]["text"].get<std::string>();
  BOOST_CHECK_NE(text.find("Exit code: 0"), std::string::npos);
  // `--help` prints the boost::program_options usage; "--help" itself and
  // "Options:" both appear in the output regardless of build config.
  BOOST_CHECK_NE(text.find("--help"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(mcp_server_tools_call_run_args_test) {
  // Exercises the pcb2gcode_run branch: the args-array parser and cwd
  // handling. Passing --version keeps this fast and deterministic.
  std::string input =
      "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"tools/call\","
      "\"params\":{\"name\":\"pcb2gcode_run\","
      "\"arguments\":{\"args\":[\"--version\"],\"cwd\":\"\"}}}\n";
  json resp = json::parse(run_mcp_with_input(input, PCB2GCODE_TEST_BINARY));
  BOOST_CHECK_EQUAL(resp["id"], 12);
  BOOST_CHECK_EQUAL(resp["result"]["isError"], false);
  std::string text = resp["result"]["content"][0]["text"].get<std::string>();
  BOOST_CHECK_NE(text.find("Exit code: 0"), std::string::npos);
  BOOST_CHECK_NE(text.find("Git commit:"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(mcp_server_tools_call_run_bad_arg_test) {
  // A bogus flag makes pcb2gcode exit non-zero with a stderr message, which
  // covers both the STDERR-formatting branch and isError=true.
  std::string input =
      "{\"jsonrpc\":\"2.0\",\"id\":13,\"method\":\"tools/call\","
      "\"params\":{\"name\":\"pcb2gcode_run\","
      "\"arguments\":{\"args\":[\"--this-flag-does-not-exist\"]}}}\n";
  json resp = json::parse(run_mcp_with_input(input, PCB2GCODE_TEST_BINARY));
  BOOST_CHECK_EQUAL(resp["id"], 13);
  BOOST_CHECK_EQUAL(resp["result"]["isError"], true);
  std::string text = resp["result"]["content"][0]["text"].get<std::string>();
  BOOST_CHECK_EQUAL(text.find("Exit code: 0"), std::string::npos);
  BOOST_CHECK_NE(text.find("STDERR:"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(mcp_server_tools_call_unknown_tool_test) {
  // Hits the else branch that returns JSON-RPC -32601 for unknown tool names.
  // No process is spawned, so the bare "pcb2gcode" path is fine here.
  std::string input =
      "{\"jsonrpc\":\"2.0\",\"id\":14,\"method\":\"tools/call\","
      "\"params\":{\"name\":\"does_not_exist\",\"arguments\":{}}}\n";
  json resp = json::parse(run_mcp_with_input(input));
  BOOST_CHECK_EQUAL(resp["id"], 14);
  BOOST_CHECK_EQUAL(resp["error"]["code"], -32601);
  std::string msg = resp["error"]["message"].get<std::string>();
  BOOST_CHECK_NE(msg.find("does_not_exist"), std::string::npos);
}

BOOST_AUTO_TEST_CASE(mcp_server_unknown_method_test) {
  // Hits the fallback JSON-RPC -32601 handler that fires when `method` is
  // none of initialize/ping/tools/list/tools/call/notifications/initialized.
  std::string input =
      "{\"jsonrpc\":\"2.0\",\"id\":15,\"method\":\"totally_unknown_method\","
      "\"params\":{}}\n";
  json resp = json::parse(run_mcp_with_input(input));
  BOOST_CHECK_EQUAL(resp["jsonrpc"], "2.0");
  BOOST_CHECK_EQUAL(resp["id"], 15);
  BOOST_CHECK_EQUAL(resp["error"]["code"], -32601);
  std::string msg = resp["error"]["message"].get<std::string>();
  BOOST_CHECK_NE(msg.find("totally_unknown_method"), std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
