#include "localagent/process/process.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("run_process executes /bin/echo", "[process]") {
  localagent::ProcessSpec spec;
  spec.argv = {"/bin/echo", "hello"};
  spec.timeout_ms = 5000;

  const auto result = localagent::run_process(spec);
  REQUIRE(result.exit_code == 0);
  REQUIRE_FALSE(result.timed_out);
  REQUIRE_FALSE(result.cancelled);
  REQUIRE(result.stdout_str.find("hello") != std::string::npos);
}

TEST_CASE("run_process respects cancellation", "[process]") {
  localagent::ProcessSpec spec;
  spec.argv = {"/bin/sleep", "30"};
  spec.timeout_ms = 60'000;

  localagent::CancellationToken token;
  token.cancel();

  const auto result = localagent::run_process(spec, token);
  REQUIRE(result.cancelled);
}
