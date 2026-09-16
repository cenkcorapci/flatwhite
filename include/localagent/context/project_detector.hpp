#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace localagent {

enum class ProjectKind { Unknown, Cpp, Python, Go, Scala };

struct ProjectInfo {
  ProjectKind kind{ProjectKind::Unknown};
  std::vector<std::string> build_commands;
  std::vector<std::string> test_commands;
};

class ProjectDetector {
public:
  explicit ProjectDetector(std::filesystem::path workspace_root);

  [[nodiscard]] ProjectInfo detect() const;

private:
  std::filesystem::path workspace_root_;
};

[[nodiscard]] ProjectKind detect_project(std::string_view root);
[[nodiscard]] std::string to_string(ProjectKind kind);

}  // namespace localagent
