#pragma once

#include "localagent/context/rules_loader.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace localagent {

enum class ContextItemKind { Goal, Rule, FileSnippet, Conversation, ProjectInfo };

struct ContextItem {
  ContextItemKind kind;
  std::string label;
  std::string content;
  uint32_t token_estimate{0};
};

struct ContextBuildRequest {
  std::string goal;
  std::vector<std::pair<std::string, std::string>> conversation;
  std::vector<std::string> file_paths;
  uint64_t token_budget{32000};
};

struct BuiltContext {
  std::vector<ContextItem> items;
  uint64_t total_tokens{0};
  bool truncated{false};
};

class ContextBuilder {
public:
  explicit ContextBuilder(std::filesystem::path workspace_root);

  [[nodiscard]] BuiltContext build(const ContextBuildRequest& request) const;

private:
  std::filesystem::path workspace_root_;
  [[nodiscard]] static uint32_t estimate_tokens(std::string_view text);
  [[nodiscard]] std::string read_snippet(const std::filesystem::path& path,
                                          std::size_t max_chars = 8000) const;
};

}  // namespace localagent
