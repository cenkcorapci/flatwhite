#include "localagent/tools/tool_registry.hpp"
#include "localagent/tools/web/web_search_tool.hpp"

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

  CHECK(registry.size() == 10);
  const auto descriptors = registry.list_descriptors();
  CHECK(std::is_sorted(descriptors.begin(), descriptors.end(),
                       [](const ToolDescriptor& a, const ToolDescriptor& b) {
                         return a.name < b.name;
                       }));
  REQUIRE(registry.find("read_file") != nullptr);
  REQUIRE(registry.find("write_file") != nullptr);
  REQUIRE(registry.find("glob") != nullptr);
  REQUIRE(registry.find("grep") != nullptr);
  REQUIRE(registry.find("shell") != nullptr);
  REQUIRE(registry.find("git_status") != nullptr);
  REQUIRE(registry.find("web_search") != nullptr);

  ToolRequest req{"read_file", {{"path", "note.txt"}}, "id-1"};
  const auto result = registry.dispatch(req, ctx);
  REQUIRE(result.has_value());
  CHECK(result->success);
}

namespace {

class FakeWebSearchTransport final : public WebSearchTransport {
public:
  WebSearchResponse get(const std::string& url) override {
    requested_url = url;
    return response;
  }

  std::string requested_url;
  WebSearchResponse response{.status_code = 200, .body = "deterministic results"};
};

}  // namespace

TEST_CASE("WebSearchTool URL-encodes queries and reports provider failures", "[web_search]") {
  auto transport = std::make_shared<FakeWebSearchTransport>();
  WebSearchTool tool{"http://127.0.0.1:8080/search?query={query}", transport};
  PermissionsConfig config;
  PermissionEngine permissions{std::filesystem::current_path(), config};
  ToolContext ctx{std::filesystem::current_path(), &permissions};

  const auto result =
      tool.execute(ToolRequest{"web_search", {{"query", "C++ files & tests"}}, "search-1"}, ctx);
  REQUIRE(result.success);
  CHECK(result.content == "deterministic results");
  CHECK(transport->requested_url ==
        "http://127.0.0.1:8080/search?query=C%2B%2B%20files%20%26%20tests");

  transport->response = {.status_code = 503, .body = "unavailable"};
  const auto failed =
      tool.execute(ToolRequest{"web_search", {{"query", "retry"}}, "search-2"}, ctx);
  REQUIRE_FALSE(failed.success);
  REQUIRE(failed.error.has_value());
  CHECK(failed.error->category == ErrorCategory::Network);
  CHECK(failed.error->code == "web_search_http_error");
}

TEST_CASE("ToolRegistry returns error for unknown tool", "[tool_registry]") {
  ToolRegistry registry;
  ToolContext ctx{std::filesystem::current_path()};
  ToolRequest req{"missing_tool", {}, "id-2"};
  const auto result = registry.dispatch(req, ctx);
  REQUIRE(result.has_value());
  CHECK_FALSE(result->success);
}
