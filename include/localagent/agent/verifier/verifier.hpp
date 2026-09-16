#pragma once

#include "localagent/context/project_detector.hpp"

#include <filesystem>
#include <string>

namespace localagent {

struct VerificationResult {
  bool success{false};
  std::string command;
  std::string output;
  int exit_code{-1};
};

class Verifier {
public:
  explicit Verifier(std::filesystem::path workspace_root);

  [[nodiscard]] VerificationResult verify_build() const;
  [[nodiscard]] VerificationResult verify_tests() const;

private:
  std::filesystem::path workspace_root_;
  ProjectInfo project_;
};

}  // namespace localagent
