#include <catch2/catch_test_macros.hpp>

#include "localagent/common/redactor.hpp"

TEST_CASE("redact_secrets masks env assignments", "[redactor]") {
  const std::string input = "API_KEY=super-secret\nDEBUG=true\n";
  const auto output = localagent::redact_secrets(input);
  REQUIRE(output.find("super-secret") == std::string::npos);
  REQUIRE(output.find("API_KEY=[REDACTED]") != std::string::npos);
  REQUIRE(output.find("DEBUG=true") != std::string::npos);
}

TEST_CASE("redact_secrets masks AWS and token patterns", "[redactor]") {
  const std::string input =
      "key=AKIAIOSFODNN7EXAMPLE token=ghp_abcdefghijklmnopqrstuvwxyz1234567890";
  const auto output = localagent::redact_secrets(input);
  REQUIRE(output.find("AKIAIOSFODNN7EXAMPLE") == std::string::npos);
  REQUIRE(output.find("ghp_abcdefghijklmnopqrstuvwxyz1234567890") == std::string::npos);
}

TEST_CASE("redact_secrets masks PEM blocks", "[redactor]") {
  const std::string input = R"(-----BEGIN PRIVATE KEY-----
abc
-----END PRIVATE KEY-----)";
  const auto output = localagent::redact_secrets(input);
  REQUIRE(output.find("BEGIN PRIVATE KEY") == std::string::npos);
  REQUIRE(output.find("[REDACTED]") != std::string::npos);
}
