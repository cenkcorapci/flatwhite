#include <catch2/catch_test_macros.hpp>

#include "localagent/common/error.hpp"

TEST_CASE("to_string covers error categories", "[error]") {
  REQUIRE(localagent::to_string(localagent::ErrorCategory::User) == "User");
  REQUIRE(localagent::to_string(localagent::ErrorCategory::Configuration) == "Configuration");
  REQUIRE(localagent::to_string(localagent::ErrorCategory::Permission) == "Permission");
  REQUIRE(localagent::to_string(localagent::ErrorCategory::Internal) == "Internal");
}

TEST_CASE("make_error builds structured errors", "[error]") {
  const auto error = localagent::make_error(localagent::ErrorCategory::Network, "timeout",
                                             "request timed out", true);
  REQUIRE(error.category == localagent::ErrorCategory::Network);
  REQUIRE(error.code == "timeout");
  REQUIRE(error.message == "request timed out");
  REQUIRE(error.retryable);
}
