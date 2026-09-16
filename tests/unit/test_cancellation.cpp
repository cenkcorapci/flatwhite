#include <catch2/catch_test_macros.hpp>

#include "localagent/common/cancellation.hpp"

TEST_CASE("CancellationToken starts uncancelled", "[cancellation]") {
  localagent::CancellationToken token;
  REQUIRE_FALSE(token.is_cancelled());
  REQUIRE_NOTHROW(token.throw_if_cancelled());
}

TEST_CASE("CancellationToken cancel propagates", "[cancellation]") {
  localagent::CancellationToken token;
  token.cancel();
  REQUIRE(token.is_cancelled());
  REQUIRE_THROWS_AS(token.throw_if_cancelled(), localagent::CancellationRequested);
}

TEST_CASE("CancellationToken copies share state", "[cancellation]") {
  localagent::CancellationToken first;
  const auto second = first;
  first.cancel();
  REQUIRE(second.is_cancelled());
}
