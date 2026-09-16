#include "localagent/tools/shell/shell_tool.hpp"

#include "localagent/common/json_util.hpp"
#include "localagent/permissions/command_classifier.hpp"
#include "localagent/process/process.hpp"

#include <cctype>
#include <vector>

namespace localagent {
namespace {

std::vector<std::string> tokenize_for_classification(std::string_view command) {
  std::vector<std::string> tokens;
  std::string current;
  bool in_single = false;
  bool in_double = false;
  for (size_t i = 0; i < command.size(); ++i) {
    const char c = command[i];
    if (c == '\'' && !in_double) {
      in_single = !in_single;
      continue;
    }
    if (c == '"' && !in_single) {
      in_double = !in_double;
      continue;
    }
    if (!in_single && !in_double && (c == '|' || c == ';' || c == '&' || c == '\n')) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      tokens.emplace_back(1, c);
      continue;
    }
    if (!in_single && !in_double && std::isspace(static_cast<unsigned char>(c))) {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(c);
  }
  if (!current.empty()) {
    tokens.push_back(current);
  }
  return tokens;
}

}  // namespace

ToolDescriptor ShellTool::descriptor() const {
  return ToolDescriptor{
      "shell",
      "Run a shell command in the workspace using fork/exec (never std::system).",
      {{"type", "object"},
       {"properties",
        {{"command", {{"type", "string"}}},
         {"timeout_seconds", {{"type", "integer"}, {"default", 300}}}}},
       {"required", nlohmann::json::array({"command"})}}};
}

ToolResult ShellTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto command = json_string(request.arguments, "command");
  if (!command) {
    return ToolResult{false,
                      {},
                      {},
                      make_error(ErrorCategory::Tool, "missing_command", "missing command")};
  }
  ctx.cancellation.throw_if_cancelled();

  const auto tokens = tokenize_for_classification(*command);
  const auto classification = classify_command(tokens.empty() ? std::vector<std::string>{*command}
                                                              : tokens);
  const auto risk = classification.primary_risk;
  if (ctx.permissions) {
    const auto decision = ctx.permissions->evaluate(risk, ctx.workspace_root);
    if (!ctx.permissions->allows(decision.policy)) {
      return ToolResult{false,
                        {},
                        {},
                        make_error(ErrorCategory::Permission, "denied",
                                   "shell command denied: " + decision.reason)};
    }
  }

  const int timeout = json_int(request.arguments, "timeout_seconds").value_or(300);
  const auto result = run_process({"/bin/sh", "-c", *command}, ctx.workspace_root,
                                    ctx.cancellation, std::chrono::seconds{timeout});

  nlohmann::json meta{{"exit_code", result.exit_code},
                      {"timed_out", result.timed_out},
                      {"risk", to_string(risk)}};
  const bool success = result.exit_code == 0 && !result.timed_out;
  std::string content = "stdout:\n" + result.stdout_str + "\nstderr:\n" + result.stderr_str;
  return ToolResult{success, std::move(content), std::move(meta), std::nullopt};
}

std::unique_ptr<Tool> make_shell_tool() { return std::make_unique<ShellTool>(); }

}  // namespace localagent
