#include "localagent/context/project_detector.hpp"

namespace localagent {

ProjectDetector::ProjectDetector(std::filesystem::path workspace_root)
    : workspace_root_(std::move(workspace_root)) {}

std::string to_string(ProjectKind kind) {
  switch (kind) {
    case ProjectKind::Cpp:
      return "cpp";
    case ProjectKind::Python:
      return "python";
    case ProjectKind::Go:
      return "go";
    case ProjectKind::Scala:
      return "scala";
    case ProjectKind::Unknown:
      return "unknown";
  }
  return "unknown";
}

ProjectKind detect_project(std::string_view root) {
  return ProjectDetector(std::filesystem::path{root}).detect().kind;
}

ProjectInfo ProjectDetector::detect() const {
  ProjectInfo info;
  if (std::filesystem::exists(workspace_root_ / "CMakeLists.txt") ||
      std::filesystem::exists(workspace_root_ / "compile_commands.json")) {
    info.kind = ProjectKind::Cpp;
    info.build_commands = {"cmake -S . -B build", "cmake --build build"};
    info.test_commands = {"ctest --test-dir build --output-on-failure"};
    return info;
  }
  if (std::filesystem::exists(workspace_root_ / "pyproject.toml") ||
      std::filesystem::exists(workspace_root_ / "setup.py") ||
      std::filesystem::exists(workspace_root_ / "requirements.txt")) {
    info.kind = ProjectKind::Python;
    info.build_commands = {"python -m pip install -e ."};
    info.test_commands = {"pytest -q"};
    return info;
  }
  if (std::filesystem::exists(workspace_root_ / "go.mod")) {
    info.kind = ProjectKind::Go;
    info.build_commands = {"go build ./..."};
    info.test_commands = {"go test ./..."};
    return info;
  }
  if (std::filesystem::exists(workspace_root_ / "build.sbt")) {
    info.kind = ProjectKind::Scala;
    info.build_commands = {"sbt compile"};
    info.test_commands = {"sbt test"};
    return info;
  }
  return info;
}

}  // namespace localagent
