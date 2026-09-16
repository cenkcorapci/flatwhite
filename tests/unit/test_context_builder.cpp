#include "localagent/context/context_builder.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace localagent;

namespace {

std::filesystem::path workspace() {
  const auto root = std::filesystem::temp_directory_path() / "localagent_ctx_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::ofstream(root / "AGENTS.md") << "Always write tests.";
  std::ofstream(root / "src.cpp") << std::string(20000, 'x');
  return root;
}

}  // namespace

TEST_CASE("ContextBuilder respects token budget and never dumps entire repo", "[context_builder]") {
  const auto root = workspace();
  ContextBuilder builder(root);
  ContextBuildRequest req;
  req.goal = "implement feature";
  req.file_paths = {"src.cpp", "missing.txt"};
  req.token_budget = 500;

  const auto built = builder.build(req);
  CHECK(built.truncated);
  CHECK(built.total_tokens <= req.token_budget);
  CHECK(built.items.size() >= 2);
  bool has_goal = false;
  bool has_rule = false;
  for (const auto& item : built.items) {
    if (item.kind == ContextItemKind::Goal) {
      has_goal = true;
    }
    if (item.kind == ContextItemKind::Rule) {
      has_rule = true;
    }
    if (item.kind == ContextItemKind::FileSnippet) {
      CHECK(item.content.size() < 20000);
    }
  }
  CHECK(has_goal);
  CHECK(has_rule);
}
