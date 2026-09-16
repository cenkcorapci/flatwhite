#include "localagent/tools/filesystem/patch_engine.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace localagent::patch;

namespace {

std::filesystem::path temp_dir() {
  const auto base = std::filesystem::temp_directory_path() /
                    ("localagent_patch_test_" + std::to_string(
                         std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(base);
  return base;
}

}  // namespace

TEST_CASE("patch exact_replace requires unique match", "[patch_engine]") {
  const auto dir = temp_dir();
  const auto file = dir / "a.txt";
  REQUIRE(create_file(file, "alpha beta alpha", false).success);

  const auto missing = exact_replace(file, "gamma", "delta");
  CHECK_FALSE(missing.success);

  const auto duplicate = exact_replace(file, "alpha", "x");
  CHECK_FALSE(duplicate.success);

  const auto ok = exact_replace(file, "beta", "BETA");
  REQUIRE(ok.success);
  std::ifstream in(file);
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(content == "alpha BETA alpha");
}

TEST_CASE("patch create_file refuses overwrite by default", "[patch_engine]") {
  const auto dir = temp_dir();
  const auto file = dir / "new.txt";
  REQUIRE(create_file(file, "one", false).success);
  const auto again = create_file(file, "two", false);
  CHECK_FALSE(again.success);
}

TEST_CASE("patch apply_unified_diff replaces hunk", "[patch_engine]") {
  const auto dir = temp_dir();
  const auto file = dir / "code.cpp";
  REQUIRE(create_file(file, "int main() {\n  return 0;\n}\n", false).success);

  const std::string diff = R"(
@@
 int main() {
-  return 0;
+  return 1;
 }
)";
  const auto result = apply_unified_diff(file, diff);
  REQUIRE(result.success);
  std::ifstream in(file);
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(content.find("return 1") != std::string::npos);
}
