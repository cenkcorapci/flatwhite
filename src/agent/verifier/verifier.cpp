#include "localagent/agent/verifier/verifier.hpp"

#include "localagent/process/process.hpp"

namespace localagent {

Verifier::Verifier(std::filesystem::path workspace_root)
    : workspace_root_(std::move(workspace_root)), project_(ProjectDetector(workspace_root_).detect()) {}

VerificationResult Verifier::verify_build() const {
  VerificationResult result;
  if (project_.build_commands.empty()) {
    result.success = true;
    result.output = "no build command detected";
    return result;
  }
  result.command = project_.build_commands.front();
  const auto proc = run_process({"/bin/sh", "-c", result.command}, workspace_root_);
  result.exit_code = proc.exit_code;
  result.output = proc.stdout_str + proc.stderr_str;
  result.success = proc.exit_code == 0;
  return result;
}

VerificationResult Verifier::verify_tests() const {
  VerificationResult result;
  if (project_.test_commands.empty()) {
    result.success = true;
    result.output = "no test command detected";
    return result;
  }
  result.command = project_.test_commands.front();
  const auto proc = run_process({"/bin/sh", "-c", result.command}, workspace_root_);
  result.exit_code = proc.exit_code;
  result.output = proc.stdout_str + proc.stderr_str;
  result.success = proc.exit_code == 0;
  return result;
}

}  // namespace localagent
