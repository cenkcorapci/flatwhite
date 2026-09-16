#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace localagent {

struct RuleFile {
  std::filesystem::path path;
  std::string content;
  std::vector<std::string> globs;
};

class RulesLoader {
public:
  explicit RulesLoader(std::filesystem::path workspace_root);

  [[nodiscard]] std::vector<RuleFile> load_all() const;
  [[nodiscard]] std::vector<RuleFile> match_for_path(std::string_view relative_path) const;

private:
  std::filesystem::path workspace_root_;
  [[nodiscard]] std::vector<RuleFile> load_directory_rules(
      const std::filesystem::path& dir) const;
  [[nodiscard]] static bool glob_matches(std::string_view pattern, std::string_view path);
};

}  // namespace localagent
