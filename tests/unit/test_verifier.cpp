#include "localagent/agent/verifier/verifier.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace localagent;

TEST_CASE("Verifier skips when no project commands detected", "[verifier]") {
  const auto root = std::filesystem::temp_directory_path() / "localagent_verify_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  Verifier verifier(root);
  const auto build = verifier.verify_build();
  CHECK(build.success);
  CHECK(build.command.empty());
}
