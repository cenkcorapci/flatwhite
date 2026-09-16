#include "localagent/context/project_detector.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace localagent;

TEST_CASE("ProjectDetector identifies CMake and suggests commands", "[project_detector]") {
  const auto root = std::filesystem::temp_directory_path() / "localagent_proj_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::ofstream(root / "CMakeLists.txt") << "cmake_minimum_required(VERSION 3.24)\n";

  ProjectDetector detector(root);
  const auto info = detector.detect();
  CHECK(info.kind == ProjectKind::Cpp);
  REQUIRE_FALSE(info.build_commands.empty());
  REQUIRE_FALSE(info.test_commands.empty());
}

TEST_CASE("ProjectDetector identifies Python projects", "[project_detector]") {
  const auto root = std::filesystem::temp_directory_path() / "localagent_py_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::ofstream(root / "pyproject.toml") << "[project]\nname='demo'\n";

  ProjectDetector detector(root);
  CHECK(detector.detect().kind == ProjectKind::Python);
}
