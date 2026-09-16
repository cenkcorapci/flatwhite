#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace localagent {

enum class PolicySetting { Allow, Ask, Deny, Sandbox };

[[nodiscard]] std::string to_string(PolicySetting policy);
[[nodiscard]] std::optional<PolicySetting> parse_policy(std::string_view text);

struct AgentConfig {
  std::string default_mode{"agent"};
  uint32_t max_turns{40};
};

struct ContextConfig {
  uint32_t default_tokens{32768};
  uint32_t max_tokens{65536};
};

struct ModelsConfig {
  std::string backend{"ollama"};
  bool auto_route{true};
  double memory_budget_gb{20.0};
};

struct PermissionsConfig {
  PolicySetting default_read{PolicySetting::Allow};
  PolicySetting workspace_write{PolicySetting::Allow};
  PolicySetting network{PolicySetting::Ask};
  PolicySetting package_install{PolicySetting::Ask};
  PolicySetting outside_workspace_write{PolicySetting::Deny};
  PolicySetting credentials{PolicySetting::Deny};
  PolicySetting destructive{PolicySetting::Ask};
  PolicySetting privilege{PolicySetting::Deny};
};

struct ExecutionConfig {
  uint32_t timeout_seconds{120};
  uint32_t max_parallel_tools{4};
};

struct Config {
  AgentConfig agent;
  ContextConfig context;
  ModelsConfig models;
  PermissionsConfig permissions;
  ExecutionConfig execution;

  std::filesystem::path workspace{"."};
  std::filesystem::path workspace_root{std::filesystem::current_path()};
  std::filesystem::path data_dir;
  std::string ollama_host{"http://127.0.0.1:11434"};
  std::string default_model{"llama3.2"};
  bool use_fake_model{false};
};

struct ConfigPaths {
  std::optional<std::filesystem::path> user_config;
  std::optional<std::filesystem::path> project_config;
};

[[nodiscard]] Config default_config();
void merge(Config& base, const Config& overlay);
[[nodiscard]] Config load_config(const ConfigPaths& paths = {});
[[nodiscard]] Config load_config(int argc, char** argv);
[[nodiscard]] std::filesystem::path default_data_dir();

}  // namespace localagent
