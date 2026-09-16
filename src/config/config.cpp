#include "localagent/config/config.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

#if defined(__APPLE__)
#include <crt_externs.h>
#elif !defined(_WIN32)
// POSIX environ must be declared at global scope; a namespaced `extern` binds
// a different symbol and fails to link under GCC/libstdc++.
extern char** environ;
#endif

namespace localagent {

namespace {

std::string trim(std::string_view text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  const auto end = text.find_last_not_of(" \t\r\n");
  return std::string{text.substr(begin, end - begin + 1)};
}

std::string to_lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::optional<bool> parse_bool(std::string_view text) {
  const auto lowered = to_lower(std::string{text});
  if (lowered == "true" || lowered == "yes" || lowered == "1") {
    return true;
  }
  if (lowered == "false" || lowered == "no" || lowered == "0") {
    return false;
  }
  return std::nullopt;
}

std::optional<double> parse_number(std::string_view text) {
  try {
    size_t consumed = 0;
    const auto value = std::stod(std::string{text}, &consumed);
    if (consumed != text.size()) {
      return std::nullopt;
    }
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<std::string> parse_quoted_string(std::string_view text) {
  const auto trimmed = trim(text);
  if (trimmed.size() < 2) {
    return std::nullopt;
  }
  const char quote = trimmed.front();
  if ((quote != '"' && quote != '\'') || trimmed.back() != quote) {
    return std::nullopt;
  }
  return trimmed.substr(1, trimmed.size() - 2);
}

struct TomlDocument {
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sections;
};

TomlDocument parse_toml(std::string_view content) {
  TomlDocument document;
  std::string current_section;

  std::istringstream stream{std::string{content}};
  std::string line;
  while (std::getline(stream, line)) {
    const auto comment = line.find('#');
    if (comment != std::string::npos) {
      line = line.substr(0, comment);
    }
    line = trim(line);
    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = to_lower(trim(line.substr(1, line.size() - 2)));
      document.sections[current_section];
      continue;
    }

    const auto equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }

    auto key = to_lower(trim(line.substr(0, equals)));
    auto raw_value = trim(line.substr(equals + 1));
    document.sections[current_section][std::move(key)] = std::move(raw_value);
  }

  return document;
}

std::optional<std::string> lookup_value(const TomlDocument& document,
                                        std::string_view section,
                                        std::string_view key) {
  const auto section_it = document.sections.find(std::string{section});
  if (section_it == document.sections.end()) {
    return std::nullopt;
  }
  const auto key_it = section_it->second.find(std::string{key});
  if (key_it == section_it->second.end()) {
    return std::nullopt;
  }
  return key_it->second;
}

std::string unquote(std::string_view raw) {
  if (auto quoted = parse_quoted_string(raw)) {
    return *quoted;
  }
  return std::string{raw};
}

PolicySetting parse_policy_or(PolicySetting fallback, std::string_view raw) {
  if (auto parsed = parse_policy(unquote(raw))) {
    return *parsed;
  }
  return fallback;
}

void apply_toml(Config& config, const TomlDocument& document) {
  if (auto value = lookup_value(document, "agent", "default_mode")) {
    config.agent.default_mode = unquote(*value);
  }
  if (auto value = lookup_value(document, "agent", "max_turns")) {
    if (auto number = parse_number(unquote(*value))) {
      config.agent.max_turns = static_cast<uint32_t>(*number);
    }
  }

  if (auto value = lookup_value(document, "context", "default_tokens")) {
    if (auto number = parse_number(unquote(*value))) {
      config.context.default_tokens = static_cast<uint32_t>(*number);
    }
  }
  if (auto value = lookup_value(document, "context", "max_tokens")) {
    if (auto number = parse_number(unquote(*value))) {
      config.context.max_tokens = static_cast<uint32_t>(*number);
    }
  }

  if (auto value = lookup_value(document, "models", "backend")) {
    config.models.backend = unquote(*value);
  }
  if (auto value = lookup_value(document, "models", "auto_route")) {
    if (auto boolean = parse_bool(unquote(*value))) {
      config.models.auto_route = *boolean;
    }
  }
  if (auto value = lookup_value(document, "models", "memory_budget_gb")) {
    if (auto number = parse_number(unquote(*value))) {
      config.models.memory_budget_gb = *number;
    }
  }

  if (auto value = lookup_value(document, "permissions", "default_read")) {
    config.permissions.default_read =
        parse_policy_or(config.permissions.default_read, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "workspace_write")) {
    config.permissions.workspace_write =
        parse_policy_or(config.permissions.workspace_write, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "network")) {
    config.permissions.network =
        parse_policy_or(config.permissions.network, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "package_install")) {
    config.permissions.package_install =
        parse_policy_or(config.permissions.package_install, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "outside_workspace_write")) {
    config.permissions.outside_workspace_write =
        parse_policy_or(config.permissions.outside_workspace_write, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "credentials")) {
    config.permissions.credentials =
        parse_policy_or(config.permissions.credentials, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "destructive")) {
    config.permissions.destructive =
        parse_policy_or(config.permissions.destructive, unquote(*value));
  }
  if (auto value = lookup_value(document, "permissions", "privilege")) {
    config.permissions.privilege =
        parse_policy_or(config.permissions.privilege, unquote(*value));
  }

  if (auto value = lookup_value(document, "execution", "timeout_seconds")) {
    if (auto number = parse_number(unquote(*value))) {
      config.execution.timeout_seconds = static_cast<uint32_t>(*number);
    }
  }
  if (auto value = lookup_value(document, "execution", "max_parallel_tools")) {
    if (auto number = parse_number(unquote(*value))) {
      config.execution.max_parallel_tools = static_cast<uint32_t>(*number);
    }
  }
}

Config load_toml_file(const std::filesystem::path& path) {
  std::ifstream input{path};
  if (!input) {
    throw std::runtime_error("failed to open config file: " + path.string());
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  Config config = default_config();
  apply_toml(config, parse_toml(buffer.str()));
  return config;
}

void apply_env_override(Config& config, std::string_view env_name, std::string_view value) {
  constexpr std::string_view prefix = "LOCALAGENT_";
  if (!env_name.starts_with(prefix)) {
    return;
  }
  const auto key = to_lower(std::string{env_name.substr(prefix.size())});

  if (key == "agent_default_mode") {
    config.agent.default_mode = std::string{value};
    return;
  }
  if (key == "agent_max_turns") {
    if (auto number = parse_number(value)) {
      config.agent.max_turns = static_cast<uint32_t>(*number);
    }
    return;
  }
  if (key == "context_default_tokens") {
    if (auto number = parse_number(value)) {
      config.context.default_tokens = static_cast<uint32_t>(*number);
    }
    return;
  }
  if (key == "context_max_tokens") {
    if (auto number = parse_number(value)) {
      config.context.max_tokens = static_cast<uint32_t>(*number);
    }
    return;
  }
  if (key == "models_backend") {
    config.models.backend = std::string{value};
    return;
  }
  if (key == "models_auto_route") {
    if (auto boolean = parse_bool(value)) {
      config.models.auto_route = *boolean;
    }
    return;
  }
  if (key == "models_memory_budget_gb") {
    if (auto number = parse_number(value)) {
      config.models.memory_budget_gb = *number;
    }
    return;
  }
  if (key == "permissions_default_read") {
    config.permissions.default_read = parse_policy_or(config.permissions.default_read, value);
    return;
  }
  if (key == "permissions_workspace_write") {
    config.permissions.workspace_write =
        parse_policy_or(config.permissions.workspace_write, value);
    return;
  }
  if (key == "permissions_network") {
    config.permissions.network = parse_policy_or(config.permissions.network, value);
    return;
  }
  if (key == "permissions_package_install") {
    config.permissions.package_install =
        parse_policy_or(config.permissions.package_install, value);
    return;
  }
  if (key == "permissions_outside_workspace_write") {
    config.permissions.outside_workspace_write =
        parse_policy_or(config.permissions.outside_workspace_write, value);
    return;
  }
  if (key == "permissions_credentials") {
    config.permissions.credentials = parse_policy_or(config.permissions.credentials, value);
    return;
  }
  if (key == "permissions_destructive") {
    config.permissions.destructive = parse_policy_or(config.permissions.destructive, value);
    return;
  }
  if (key == "permissions_privilege") {
    config.permissions.privilege = parse_policy_or(config.permissions.privilege, value);
    return;
  }
  if (key == "execution_timeout_seconds") {
    if (auto number = parse_number(value)) {
      config.execution.timeout_seconds = static_cast<uint32_t>(*number);
    }
    return;
  }
  if (key == "execution_max_parallel_tools") {
    if (auto number = parse_number(value)) {
      config.execution.max_parallel_tools = static_cast<uint32_t>(*number);
    }
    return;
  }
  if (key == "fake_model") {
    if (auto boolean = parse_bool(value)) {
      config.use_fake_model = *boolean;
    }
    return;
  }
  if (key == "ollama_host") {
    config.ollama_host = std::string{value};
    return;
  }
  if (key == "default_model") {
    config.default_model = std::string{value};
    return;
  }
  if (key == "data_dir") {
    config.data_dir = std::string{value};
    return;
  }
}

void apply_env_overrides(Config& config) {
#if defined(_WIN32)
  // Skip on Windows for now; POSIX environments are the primary target.
#elif defined(__APPLE__)
  char** envp = *_NSGetEnviron();
  for (char** env = envp; env != nullptr && *env != nullptr; ++env) {
    const std::string_view entry{*env};
    const auto equals = entry.find('=');
    if (equals == std::string_view::npos) {
      continue;
    }
    apply_env_override(config, entry.substr(0, equals), entry.substr(equals + 1));
  }
#else
  for (char** env = ::environ; env != nullptr && *env != nullptr; ++env) {
    const std::string_view entry{*env};
    const auto equals = entry.find('=');
    if (equals == std::string_view::npos) {
      continue;
    }
    apply_env_override(config, entry.substr(0, equals), entry.substr(equals + 1));
  }
#endif
}

std::filesystem::path default_user_config_path() {
  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path{home} / ".config" / "localagent" / "config.toml";
  }
  return std::filesystem::path{".config/localagent/config.toml"};
}

}  // namespace

std::string to_string(PolicySetting policy) {
  switch (policy) {
    case PolicySetting::Allow:
      return "allow";
    case PolicySetting::Ask:
      return "ask";
    case PolicySetting::Deny:
      return "deny";
    case PolicySetting::Sandbox:
      return "sandbox";
  }
  return "deny";
}

std::optional<PolicySetting> parse_policy(std::string_view text) {
  const auto lowered = to_lower(std::string{text});
  if (lowered == "allow") {
    return PolicySetting::Allow;
  }
  if (lowered == "ask") {
    return PolicySetting::Ask;
  }
  if (lowered == "deny") {
    return PolicySetting::Deny;
  }
  if (lowered == "sandbox") {
    return PolicySetting::Sandbox;
  }
  return std::nullopt;
}

Config default_config() { return Config{}; }

void merge(Config& base, const Config& overlay) {
  if (!overlay.agent.default_mode.empty()) {
    base.agent.default_mode = overlay.agent.default_mode;
  }
  if (overlay.agent.max_turns != 0) {
    base.agent.max_turns = overlay.agent.max_turns;
  }

  if (overlay.context.default_tokens != 0) {
    base.context.default_tokens = overlay.context.default_tokens;
  }
  if (overlay.context.max_tokens != 0) {
    base.context.max_tokens = overlay.context.max_tokens;
  }

  if (!overlay.models.backend.empty()) {
    base.models.backend = overlay.models.backend;
  }
  base.models.auto_route = overlay.models.auto_route;
  if (overlay.models.memory_budget_gb > 0.0) {
    base.models.memory_budget_gb = overlay.models.memory_budget_gb;
  }

  base.permissions = overlay.permissions;
  base.execution = overlay.execution;

  if (!overlay.workspace.empty()) {
    base.workspace = overlay.workspace;
    base.workspace_root = overlay.workspace;
  }
  if (!overlay.workspace_root.empty()) {
    base.workspace_root = overlay.workspace_root;
  }
  if (!overlay.data_dir.empty()) {
    base.data_dir = overlay.data_dir;
  }
  if (!overlay.ollama_host.empty()) {
    base.ollama_host = overlay.ollama_host;
  }
  if (!overlay.default_model.empty()) {
    base.default_model = overlay.default_model;
  }
  base.use_fake_model = overlay.use_fake_model;
}

Config load_config(const ConfigPaths& paths) {
  Config config = default_config();

  const auto user_path = paths.user_config.value_or(default_user_config_path());
  if (std::filesystem::exists(user_path)) {
    merge(config, load_toml_file(user_path));
  }

  const auto project_path = paths.project_config.value_or(std::filesystem::path{".agent/config.toml"});
  if (std::filesystem::exists(project_path)) {
    merge(config, load_toml_file(project_path));
  }

  apply_env_overrides(config);
  config.workspace_root = config.workspace.empty() ? std::filesystem::current_path() : config.workspace;
  if (config.data_dir.empty()) {
    config.data_dir = default_data_dir();
  }
  if (!config.agent.default_mode.empty()) {
    config.default_model = config.default_model.empty() ? "llama3.2" : config.default_model;
  }
  return config;
}

std::filesystem::path default_data_dir() {
  if (const char* home = std::getenv("HOME")) {
    return std::filesystem::path{home} / ".localagent";
  }
  return std::filesystem::path{".localagent"};
}

Config load_config(int argc, char** argv) {
  ConfigPaths paths;
  Config config = load_config(paths);

  for (int i = 1; i < argc; ++i) {
    const std::string arg{argv[i]};
    if (arg == "--fake-model") {
      config.use_fake_model = true;
    } else if (arg == "--workspace" && i + 1 < argc) {
      config.workspace_root = argv[++i];
      config.workspace = config.workspace_root;
    } else if (arg == "--data-dir" && i + 1 < argc) {
      config.data_dir = argv[++i];
    }
  }

  if (config.data_dir.empty()) {
    config.data_dir = default_data_dir();
  }
  return config;
}

}  // namespace localagent
