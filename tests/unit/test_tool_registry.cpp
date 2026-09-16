#include "localagent/tools/tool_registry.hpp"

#include <algorithm>
#include <fstream>

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

TEST_CASE("ToolRegistry registers defaults and dispatches by name", "[tool_registry]") {
  const auto root = std::filesystem::temp_directory_path() / "localagent_registry_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::ofstream(root / "note.txt") << "registry";

  ToolRegistry registry;
  ToolContext ctx{root, nullptr, CancellationToken{}};
  registry.register_defaults(ctx);

  CHECK(registry.size() >= 8);
  const auto descriptors = registry.list_descriptors();
  CHECK(std::is_sorted(descriptors.begin(), descriptors.end(),
                       [](const ToolDescriptor& a, const ToolDescriptor& b) {
                         return a.name < b.name;
                       }));

  ToolRequest req{"read_file", {{"path", "note.txt"}}, "id-1"};
  const auto result = registry.dispatch(req, ctx);
  REQUIRE(result.has_value());
  CHECK(result->success);
}

TEST_CASE("ToolRegistry returns error for unknown tool", "[tool_registry]") {
  ToolRegistry registry;
  ToolContext ctx{std::filesystem::current_path()};
  ToolRequest req{"missing_tool", {}, "id-2"};
  const auto result = registry.dispatch(req, ctx);
  REQUIRE(result.has_value());
  CHECK_FALSE(result->success);
}
