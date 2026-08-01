#define BOOST_TEST_MODULE mcp_server tests
#include <boost/test/unit_test.hpp>

#include "mcp_server.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <iostream>

using json = nlohmann::json;

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

BOOST_AUTO_TEST_SUITE_END()
