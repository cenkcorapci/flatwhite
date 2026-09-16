#include <catch2/catch_test_macros.hpp>

#include "localagent/common/json_util.hpp"

TEST_CASE("parse_or_throw accepts valid JSON", "[json_util]") {
  const auto value = localagent::parse_or_throw(R"({"name":"localagent","count":3})");
  REQUIRE(value.at("name") == "localagent");
  REQUIRE(value.at("count") == 3);
}

TEST_CASE("parse_or_throw rejects invalid JSON", "[json_util]") {
  REQUIRE_THROWS_AS(localagent::parse_or_throw("{not json"), std::runtime_error);
}

TEST_CASE("dump_compact produces compact output", "[json_util]") {
  nlohmann::json value{{"a", 1}, {"b", "two"}};
  REQUIRE(localagent::dump_compact(value) == R"({"a":1,"b":"two"})");
}

TEST_CASE("get_string_or handles missing and typed values", "[json_util]") {
  const nlohmann::json value{
      {"name", "agent"},
      {"count", 42},
      {"enabled", true},
  };

  REQUIRE(localagent::get_string_or(value, "name", "fallback") == "agent");
  REQUIRE(localagent::get_string_or(value, "missing", "fallback") == "fallback");
  REQUIRE(localagent::get_string_or(value, "count") == "42");
  REQUIRE(localagent::get_string_or(value, "enabled") == "true");
}
