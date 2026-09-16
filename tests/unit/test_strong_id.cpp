#include <catch2/catch_test_macros.hpp>

#include "localagent/common/strong_id.hpp"

#include <regex>
#include <set>

TEST_CASE("StrongId comparison and hashing", "[strong_id]") {
  localagent::SessionId a{"session-a"};
  localagent::SessionId b{"session-b"};
  localagent::SessionId c{"session-a"};

  REQUIRE(a == c);
  REQUIRE(a != b);
  REQUIRE(a < b);

  std::hash<localagent::SessionId> hasher;
  REQUIRE(hasher(a) == hasher(c));
}

TEST_CASE("make_uuid format and uniqueness", "[strong_id]") {
  static const std::regex uuid_pattern(
      R"(^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$)");

  std::set<std::string> seen;
  for (int i = 0; i < 32; ++i) {
    const auto id = localagent::make_uuid();
    REQUIRE_FALSE(id.empty());
    REQUIRE(std::regex_match(id, uuid_pattern));
    REQUIRE(seen.insert(id).second);
  }
}
