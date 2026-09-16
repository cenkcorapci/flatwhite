#include "localagent/common/error.hpp"

namespace localagent {

std::string to_string(ErrorCategory category) {
  switch (category) {
    case ErrorCategory::User:
      return "User";
    case ErrorCategory::Configuration:
      return "Configuration";
    case ErrorCategory::Model:
      return "Model";
    case ErrorCategory::Tool:
      return "Tool";
    case ErrorCategory::Process:
      return "Process";
    case ErrorCategory::Permission:
      return "Permission";
    case ErrorCategory::Resource:
      return "Resource";
    case ErrorCategory::Network:
      return "Network";
    case ErrorCategory::Persistence:
      return "Persistence";
    case ErrorCategory::Internal:
      return "Internal";
  }
  return "Internal";
}

AgentError make_error(ErrorCategory category, std::string code, std::string message,
                      bool retryable) {
  return AgentError{
      .category = category,
      .code = std::move(code),
      .message = std::move(message),
      .retryable = retryable,
  };
}

}  // namespace localagent
