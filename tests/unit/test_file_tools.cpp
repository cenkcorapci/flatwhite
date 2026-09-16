#include "localagent/tools/filesystem/file_tools.hpp"
#include "localagent/tools/tool_registry.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace localagent;

namespace {

std::filesystem::path workspace() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("localagent_file_tools_test_" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  std::ofstream(root / "hello.txt") << "hello world\n";
  std::ofstream(root / "needle.cpp") << "int needle = 42;\n";
  return root;
}

}  // namespace

TEST_CASE("file tools resolve workspace paths and reject escape", "[file_tools]") {
  const auto root = workspace();
  ToolContext ctx{root, nullptr, CancellationToken{}};

  ToolRequest read_req{"read_file", {{"path", "hello.txt"}}, "1"};
  ReadFileTool read_tool;
  const auto read = read_tool.execute(read_req, ctx);
  REQUIRE(read.success);
  CHECK(read.content.find("hello") != std::string::npos);

  ToolRequest bad_req{"read_file", {{"path", "../outside.txt"}}, "2"};
  const auto denied = read_tool.execute(bad_req, ctx);
  CHECK_FALSE(denied.success);
}

TEST_CASE("grep and glob search within workspace", "[file_tools]") {
  const auto root = workspace();
  ToolContext ctx{root};

  GlobTool glob;
  const auto glob_result = glob.execute(ToolRequest{"glob", {{"pattern", "*.cpp"}}, "3"}, ctx);
  REQUIRE(glob_result.success);
  CHECK(glob_result.content.find("needle.cpp") != std::string::npos);

  GrepTool grep;
  const auto grep_result =
      grep.execute(ToolRequest{"grep", {{"pattern", "needle"}}, "4"}, ctx);
  REQUIRE(grep_result.success);
  CHECK(grep_result.content.find("42") != std::string::npos);
}

TEST_CASE("write_file creates content atomically", "[file_tools]") {
  const auto root = workspace();
  ToolContext ctx{root};

  WriteFileTool write;
  const auto result = write.execute(
      ToolRequest{"write_file", {{"path", "out.txt"}, {"content", "data"}}, "5"}, ctx);
  REQUIRE(result.success);
  std::ifstream in(root / "out.txt");
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(content == "data");
}
