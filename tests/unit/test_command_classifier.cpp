#include <catch2/catch_test_macros.hpp>

#include "localagent/permissions/command_classifier.hpp"

TEST_CASE("classify_command marks git read operations", "[command_classifier]") {
  const auto status = localagent::classify_command({"git", "status"});
  REQUIRE(status.primary_risk == localagent::RiskLevel::Read);
  REQUIRE(status.summary == "git read");

  const auto diff = localagent::classify_command({"git", "diff", "--stat"});
  REQUIRE(diff.primary_risk == localagent::RiskLevel::Read);
}

TEST_CASE("classify_command marks destructive and privileged commands", "[command_classifier]") {
  const auto remove = localagent::classify_command({"rm", "-rf", "/tmp/x"});
  REQUIRE(remove.primary_risk == localagent::RiskLevel::Destructive);

  const auto sudo = localagent::classify_command({"sudo", "apt", "update"});
  REQUIRE(sudo.primary_risk == localagent::RiskLevel::Privilege);

  const auto dd = localagent::classify_command({"dd", "if=/dev/zero", "of=/dev/sda"});
  REQUIRE(dd.primary_risk == localagent::RiskLevel::Privilege);
}

TEST_CASE("classify_command marks network and package install", "[command_classifier]") {
  const auto curl = localagent::classify_command({"curl", "https://example.com/install.sh"});
  REQUIRE(curl.primary_risk == localagent::RiskLevel::Network);

  const auto pip = localagent::classify_command({"pip", "install", "requests"});
  REQUIRE(pip.primary_risk == localagent::RiskLevel::PackageInstall);
}

TEST_CASE("classify_command handles build/test commands as read", "[command_classifier]") {
  const auto cmake = localagent::classify_command({"cmake", "--build", "build"});
  REQUIRE(cmake.primary_risk == localagent::RiskLevel::Read);
  REQUIRE(cmake.summary == "build/test");
}
