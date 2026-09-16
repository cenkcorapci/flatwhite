#pragma once

#include "localagent/common/cancellation.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace localagent {

struct ProcessSpec {
  std::vector<std::string> argv;
  std::optional<std::string> cwd;
  std::map<std::string, std::string> env;
  std::uint32_t timeout_ms{0};
  bool use_pty{false};
};

struct ProcessResult {
  int exit_code{-1};
  std::string stdout_str;
  std::string stderr_str;
  bool timed_out{false};
  bool cancelled{false};
};

[[nodiscard]] ProcessResult run_process(const ProcessSpec& spec,
                                        CancellationToken token = {});

[[nodiscard]] ProcessResult run_process(
    const std::vector<std::string>& argv, const std::filesystem::path& cwd,
    const CancellationToken& token = CancellationToken{},
    std::chrono::seconds timeout = std::chrono::seconds{300});

class PosixProcessRuntime {
public:
  [[nodiscard]] ProcessResult run(const ProcessSpec& spec,
                                  CancellationToken token = {}) const {
    return run_process(spec, token);
  }
};

}  // namespace localagent
