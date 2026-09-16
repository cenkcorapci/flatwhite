#pragma once

#include "localagent/common/cancellation.hpp"
#include "localagent/common/error.hpp"
#include "localagent/permissions/permission_engine.hpp"

#include <filesystem>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace localagent {

struct ToolDescriptor {
  std::string name;
  std::string description;
  nlohmann::json parameters_schema;
};

struct ToolRequest {
  std::string tool_name;
  nlohmann::json arguments;
  std::string call_id;
};

struct ToolResult {
  bool success{false};
  std::string content;
  nlohmann::json metadata;
  std::optional<AgentError> error;
};

struct ToolContext {
  std::filesystem::path workspace_root;
  PermissionEngine* permissions{nullptr};
  CancellationToken cancellation;
};

class Tool {
public:
  virtual ~Tool() = default;
  [[nodiscard]] virtual ToolDescriptor descriptor() const = 0;
  [[nodiscard]] virtual ToolResult execute(const ToolRequest& request,
                                           const ToolContext& ctx) const = 0;
};

[[nodiscard]] std::filesystem::path resolve_workspace_path(const ToolContext& ctx,
                                                           const std::string& user_path,
                                                           bool allow_outside = false);

[[nodiscard]] bool path_within_workspace(const std::filesystem::path& workspace,
                                         const std::filesystem::path& candidate);

}  // namespace localagent
