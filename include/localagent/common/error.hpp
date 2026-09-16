#pragma once

#include <string>
#include <utility>

namespace localagent {

enum class ErrorCategory {
  User,
  Configuration,
  Model,
  Tool,
  Process,
  Permission,
  Resource,
  Network,
  Persistence,
  Internal
};

struct AgentError {
  ErrorCategory category{ErrorCategory::Internal};
  std::string code;
  std::string message;
  bool retryable{false};
};

[[nodiscard]] std::string to_string(ErrorCategory category);
[[nodiscard]] AgentError make_error(ErrorCategory category, std::string code,
                                    std::string message, bool retryable = false);

}  // namespace localagent
