#include "localagent/context/rules_loader.hpp"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace localagent;

TEST_CASE("RulesLoader loads AGENTS.md and scoped cursor rules", "[rules_loader]") {
  const auto root = std::filesystem::temp_directory_path() / "localagent_rules_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / ".cursor/rules");
  std::ofstream(root / "AGENTS.md") << "global rule";
  std::ofstream(root / ".cursor/rules/cpp.md") << "---\nglobs: *.cpp\n---\ncpp only\n";

  RulesLoader loader(root);
  const auto all = loader.load_all();
  REQUIRE(all.size() >= 2);

  const auto matched = loader.match_for_path("src/main.cpp");
  REQUIRE_FALSE(matched.empty());
  const bool has_cpp_rule = std::ranges::any_of(matched, [](const RuleFile& rule) {
    return rule.content.find("cpp only") != std::string::npos;
  });
  CHECK(has_cpp_rule);
}
