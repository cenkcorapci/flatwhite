#include <catch2/catch_test_macros.hpp>

#include "localagent/config/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path write_temp_config(std::string_view content) {
  const auto path = std::filesystem::temp_directory_path() /
                    ("localagent-test-" + std::to_string(std::rand()) + ".toml");
  std::ofstream out{path};
  out << content;
  return path;
}

}  // namespace

TEST_CASE("default_config has expected baseline", "[config]") {
  const auto config = localagent::default_config();
  REQUIRE(config.agent.default_mode == "agent");
  REQUIRE(config.agent.max_turns == 40);
  REQUIRE(config.models.backend == "ollama");
  REQUIRE(config.permissions.unrestricted);
  REQUIRE(config.permissions.outside_workspace_write == localagent::PolicySetting::Deny);
  REQUIRE(config.web_search.endpoint == "https://html.duckduckgo.com/html/?q={query}");
  REQUIRE(config.execution.timeout_seconds == 120);
}

TEST_CASE("parse_policy accepts known values", "[config]") {
  REQUIRE(localagent::parse_policy("allow") == localagent::PolicySetting::Allow);
  REQUIRE(localagent::parse_policy("ASK") == localagent::PolicySetting::Ask);
  REQUIRE(localagent::parse_policy("deny") == localagent::PolicySetting::Deny);
  REQUIRE(localagent::parse_policy("sandbox") == localagent::PolicySetting::Sandbox);
  REQUIRE_FALSE(localagent::parse_policy("maybe").has_value());
}

TEST_CASE("load_config merges project over defaults", "[config]") {
  const auto project = write_temp_config(R"(
[agent]
default_mode = "plan"
max_turns = 25

[permissions]
unrestricted = false
network = "deny"

[web_search]
endpoint = "http://127.0.0.1:8080/search?query={query}"

[execution]
timeout_seconds = 300
)");

  localagent::ConfigPaths paths;
  paths.project_config = project;
  paths.user_config = std::filesystem::path{"/nonexistent/localagent-user-config.toml"};

  const auto config = localagent::load_config(paths);
  REQUIRE(config.agent.default_mode == "plan");
  REQUIRE(config.agent.max_turns == 25);
  REQUIRE(config.permissions.network == localagent::PolicySetting::Deny);
  REQUIRE_FALSE(config.permissions.unrestricted);
  REQUIRE(config.web_search.endpoint == "http://127.0.0.1:8080/search?query={query}");
  REQUIRE(config.execution.timeout_seconds == 300);
  REQUIRE(config.models.backend == "ollama");

  std::filesystem::remove(project);
}

TEST_CASE("load_config applies FLATWHITE env overrides", "[config]") {
#if !defined(_WIN32)
  const auto previous = std::getenv("FLATWHITE_AGENT_DEFAULT_MODE");
  const auto previous_unrestricted = std::getenv("FLATWHITE_PERMISSIONS_UNRESTRICTED");
  const auto previous_endpoint = std::getenv("FLATWHITE_WEB_SEARCH_ENDPOINT");
  setenv("FLATWHITE_AGENT_DEFAULT_MODE", "ask", 1);
  setenv("FLATWHITE_PERMISSIONS_UNRESTRICTED", "false", 1);
  setenv("FLATWHITE_WEB_SEARCH_ENDPOINT", "http://127.0.0.1/search?q={query}", 1);

  localagent::ConfigPaths paths;
  paths.user_config = std::filesystem::path{"/nonexistent/localagent-user-config.toml"};
  paths.project_config = std::filesystem::path{"/nonexistent/localagent-project-config.toml"};

  const auto config = localagent::load_config(paths);
  REQUIRE(config.agent.default_mode == "ask");
  REQUIRE_FALSE(config.permissions.unrestricted);
  REQUIRE(config.web_search.endpoint == "http://127.0.0.1/search?q={query}");

  if (previous) {
    setenv("FLATWHITE_AGENT_DEFAULT_MODE", previous, 1);
  } else {
    unsetenv("FLATWHITE_AGENT_DEFAULT_MODE");
  }
  if (previous_unrestricted) {
    setenv("FLATWHITE_PERMISSIONS_UNRESTRICTED", previous_unrestricted, 1);
  } else {
    unsetenv("FLATWHITE_PERMISSIONS_UNRESTRICTED");
  }
  if (previous_endpoint) {
    setenv("FLATWHITE_WEB_SEARCH_ENDPOINT", previous_endpoint, 1);
  } else {
    unsetenv("FLATWHITE_WEB_SEARCH_ENDPOINT");
  }
#endif
}

TEST_CASE("merge overlays non-empty fields", "[config]") {
  auto base = localagent::default_config();
  localagent::Config overlay;
  overlay.agent.default_mode = "debug";
  overlay.permissions.destructive = localagent::PolicySetting::Deny;
  localagent::merge(base, overlay);
  REQUIRE(base.agent.default_mode == "debug");
  REQUIRE(base.permissions.destructive == localagent::PolicySetting::Deny);
}
