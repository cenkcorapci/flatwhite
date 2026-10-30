#include <catch2/catch_test_macros.hpp>

#include "localagent/permissions/permission_engine.hpp"

#include <filesystem>

TEST_CASE("PermissionEngine allows workspace reads", "[permissions]") {
  const auto workspace = std::filesystem::current_path();
  localagent::PermissionsConfig config;
  localagent::PermissionEngine engine{workspace, config};

  const auto decision =
      engine.evaluate(localagent::RiskLevel::Read, workspace / "src/main.cpp");
  REQUIRE(decision.policy == localagent::Policy::Allow);
  REQUIRE(engine.is_inside_workspace(workspace / "src"));
}

TEST_CASE("PermissionEngine defaults to permitting every classified risk", "[permissions]") {
  const auto workspace = std::filesystem::current_path();
  localagent::PermissionsConfig config;
  localagent::PermissionEngine engine{workspace, config};

  const auto decision = engine.evaluate(localagent::RiskLevel::WorkspaceWrite, "/tmp/outside.txt");
  REQUIRE(decision.risk == localagent::RiskLevel::OutsideWorkspaceWrite);
  REQUIRE(decision.policy == localagent::Policy::Allow);
  REQUIRE(decision.reason == "unrestricted");
  REQUIRE(engine.allows(decision.policy));
}

TEST_CASE("PermissionEngine preserves credential classification while unrestricted", "[permissions]") {
  const auto workspace = std::filesystem::current_path();
  localagent::PermissionsConfig config;
  localagent::PermissionEngine engine{workspace, config};

  const auto home = std::getenv("HOME");
  REQUIRE(home != nullptr);
  const std::filesystem::path ssh_key = std::filesystem::path{home} / ".ssh" / "id_rsa";

  REQUIRE(engine.is_credential_path(ssh_key));
  const auto decision = engine.evaluate(localagent::RiskLevel::WorkspaceWrite, ssh_key);
  REQUIRE(decision.risk == localagent::RiskLevel::Credential);
  REQUIRE(decision.policy == localagent::Policy::Allow);
  REQUIRE(decision.reason == "unrestricted");
}

TEST_CASE("PermissionEngine honors policies when unrestricted mode is disabled", "[permissions]") {
  const auto workspace = std::filesystem::current_path();
  localagent::PermissionsConfig config;
  config.unrestricted = false;
  config.network = localagent::PolicySetting::Ask;
  localagent::PermissionEngine engine{workspace, config};

  const auto decision = engine.evaluate(localagent::RiskLevel::Network);
  REQUIRE(decision.policy == localagent::Policy::Ask);
  REQUIRE_FALSE(engine.allows(decision.policy));
  engine.set_interactive(true);
  REQUIRE(engine.allows(localagent::Policy::Ask));
}
