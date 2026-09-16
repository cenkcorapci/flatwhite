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
  REQUIRE(config.permissions.outside_workspace_write == localagent::PolicySetting::Deny);
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
network = "deny"

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
  REQUIRE(config.execution.timeout_seconds == 300);
  REQUIRE(config.models.backend == "ollama");

  std::filesystem::remove(project);
}

TEST_CASE("load_config applies LOCALAGENT env overrides", "[config]") {
#if !defined(_WIN32)
  const auto previous = std::getenv("LOCALAGENT_AGENT_DEFAULT_MODE");
  setenv("LOCALAGENT_AGENT_DEFAULT_MODE", "ask", 1);

  localagent::ConfigPaths paths;
  paths.user_config = std::filesystem::path{"/nonexistent/localagent-user-config.toml"};
  paths.project_config = std::filesystem::path{"/nonexistent/localagent-project-config.toml"};

  const auto config = localagent::load_config(paths);
  REQUIRE(config.agent.default_mode == "ask");

  if (previous) {
    setenv("LOCALAGENT_AGENT_DEFAULT_MODE", previous, 1);
  } else {
    unsetenv("LOCALAGENT_AGENT_DEFAULT_MODE");
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
