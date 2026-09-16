#pragma once

#include "localagent/config/config.hpp"

#include <filesystem>
#include <string>

namespace localagent {

enum class RiskLevel {
  Read,
  WorkspaceWrite,
  Network,
  PackageInstall,
  OutsideWorkspaceWrite,
  Credential,
  Destructive,
  Privilege
};

enum class Policy { Allow, Ask, Deny, Sandbox };

[[nodiscard]] std::string to_string(RiskLevel risk);
[[nodiscard]] std::string to_string(Policy policy);

struct PermissionDecision {
  Policy policy{Policy::Deny};
  RiskLevel risk{RiskLevel::Read};
  std::string reason;
};

class PermissionEngine {
public:
  PermissionEngine() = default;
  explicit PermissionEngine(bool allow_all);
  PermissionEngine(std::filesystem::path workspace, PermissionsConfig config);

  [[nodiscard]] Policy check(RiskLevel risk, const std::filesystem::path& target = {}) const;
  [[nodiscard]] PermissionDecision evaluate(RiskLevel risk,
                                            const std::filesystem::path& target = {}) const;
  [[nodiscard]] bool allows(Policy policy) const;
  void set_allow_all(bool allow) { allow_all_ = allow; }
  void set_interactive(bool interactive) { interactive_ = interactive; }
  [[nodiscard]] bool is_inside_workspace(const std::filesystem::path& path) const;
  [[nodiscard]] bool is_credential_path(const std::filesystem::path& path) const;
  [[nodiscard]] const std::filesystem::path& workspace() const noexcept { return workspace_; }

private:
  [[nodiscard]] Policy policy_for(RiskLevel risk) const;

  std::filesystem::path workspace_;
  PermissionsConfig config_;
  bool allow_all_{false};
  bool interactive_{false};
};

}  // namespace localagent
