#include "localagent/permissions/permission_engine.hpp"

#include <algorithm>

namespace localagent {

namespace {

Policy to_policy(PolicySetting setting) {
  switch (setting) {
    case PolicySetting::Allow:
      return Policy::Allow;
    case PolicySetting::Ask:
      return Policy::Ask;
    case PolicySetting::Deny:
      return Policy::Deny;
    case PolicySetting::Sandbox:
      return Policy::Sandbox;
  }
  return Policy::Deny;
}

std::filesystem::path normalize_path(const std::filesystem::path& path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  if (!error) {
    return canonical;
  }
  const auto absolute = std::filesystem::absolute(path, error);
  if (error) {
    return path.lexically_normal();
  }
  return absolute.lexically_normal();
}

bool path_starts_with(const std::filesystem::path& path, const std::filesystem::path& prefix) {
  const auto normalized_path = normalize_path(path);
  const auto normalized_prefix = normalize_path(prefix);
  if (normalized_path == normalized_prefix) {
    return true;
  }
  const auto relative = normalized_path.lexically_relative(normalized_prefix);
  if (relative.empty() || relative.string().starts_with("..")) {
    return false;
  }
  return !relative.has_parent_path() || relative != std::filesystem::path{".."};
}

}  // namespace

std::string to_string(RiskLevel risk) {
  switch (risk) {
    case RiskLevel::Read:
      return "Read";
    case RiskLevel::WorkspaceWrite:
      return "WorkspaceWrite";
    case RiskLevel::Network:
      return "Network";
    case RiskLevel::PackageInstall:
      return "PackageInstall";
    case RiskLevel::OutsideWorkspaceWrite:
      return "OutsideWorkspaceWrite";
    case RiskLevel::Credential:
      return "Credential";
    case RiskLevel::Destructive:
      return "Destructive";
    case RiskLevel::Privilege:
      return "Privilege";
  }
  return "Read";
}

std::string to_string(Policy policy) {
  switch (policy) {
    case Policy::Allow:
      return "Allow";
    case Policy::Ask:
      return "Ask";
    case Policy::Deny:
      return "Deny";
    case Policy::Sandbox:
      return "Sandbox";
  }
  return "Deny";
}

PermissionEngine::PermissionEngine(bool allow_all)
    : workspace_(normalize_path(std::filesystem::current_path())), allow_all_(allow_all) {}

PermissionEngine::PermissionEngine(std::filesystem::path workspace, PermissionsConfig config)
    : workspace_(normalize_path(std::move(workspace))),
      config_(std::move(config)),
      allow_all_(false) {}

Policy PermissionEngine::check(RiskLevel risk, const std::filesystem::path& target) const {
  if (allow_all_) {
    return Policy::Allow;
  }
  return evaluate(risk, target).policy;
}

bool PermissionEngine::allows(Policy policy) const {
  if (allow_all_) {
    return true;
  }
  switch (policy) {
    case Policy::Allow:
      return true;
    case Policy::Ask:
      // Non-interactive agent runs must not auto-approve Ask policies.
      return interactive_;
    case Policy::Sandbox:
      // Sandbox execution is not implemented yet; fail closed.
      return false;
    case Policy::Deny:
      return false;
  }
  return false;
}

Policy PermissionEngine::policy_for(RiskLevel risk) const {
  switch (risk) {
    case RiskLevel::Read:
      return to_policy(config_.default_read);
    case RiskLevel::WorkspaceWrite:
      return to_policy(config_.workspace_write);
    case RiskLevel::Network:
      return to_policy(config_.network);
    case RiskLevel::PackageInstall:
      return to_policy(config_.package_install);
    case RiskLevel::OutsideWorkspaceWrite:
      return to_policy(config_.outside_workspace_write);
    case RiskLevel::Credential:
      return to_policy(config_.credentials);
    case RiskLevel::Destructive:
      return to_policy(config_.destructive);
    case RiskLevel::Privilege:
      return to_policy(config_.privilege);
  }
  return Policy::Deny;
}

bool PermissionEngine::is_inside_workspace(const std::filesystem::path& path) const {
  return path_starts_with(path, workspace_);
}

bool PermissionEngine::is_credential_path(const std::filesystem::path& path) const {
  const auto normalized = normalize_path(path).string();
  const auto filename = path.filename().string();

  static constexpr const char* kCredentialMarkers[] = {
      "/.ssh/",
      "/.aws/",
      "/.gnupg/",
      "/.config/gcloud/",
      "/.netrc",
      "/.npmrc",
  };

  for (const auto* marker : kCredentialMarkers) {
    if (normalized.find(marker) != std::string::npos) {
      return true;
    }
  }

  if (filename == "id_rsa" || filename == "id_ed25519" || filename == "id_ecdsa" ||
      filename == ".env" || filename.ends_with(".pem") || filename.ends_with(".key")) {
    return true;
  }

  return false;
}

PermissionDecision PermissionEngine::evaluate(RiskLevel risk,
                                              const std::filesystem::path& target) const {
  if (allow_all_) {
    return PermissionDecision{
        .policy = Policy::Allow,
        .risk = risk,
        .reason = "allow_all",
    };
  }

  PermissionDecision decision{
      .policy = policy_for(risk),
      .risk = risk,
  };

  if (!target.empty()) {
    const auto normalized = normalize_path(target).string();
    if (normalized.find("/.ssh/") != std::string::npos && risk != RiskLevel::Read) {
      decision.risk = RiskLevel::Credential;
      decision.policy = Policy::Deny;
      decision.reason = "deny write to ~/.ssh/**";
      return decision;
    }

    if (is_credential_path(target)) {
      decision.risk = RiskLevel::Credential;
      decision.policy = policy_for(RiskLevel::Credential);
      decision.reason = "credential path";
      return decision;
    }

    const bool inside = is_inside_workspace(target);
    if (!inside && (risk == RiskLevel::WorkspaceWrite || risk == RiskLevel::Read)) {
      decision.risk = RiskLevel::OutsideWorkspaceWrite;
      decision.policy = policy_for(RiskLevel::OutsideWorkspaceWrite);
      decision.reason = "outside workspace";
      return decision;
    }
  }

  if (decision.reason.empty()) {
    decision.reason = to_string(decision.risk);
  }
  return decision;
}

}  // namespace localagent
