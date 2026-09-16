#include "localagent/tools/shell/shell_tool.hpp"

#include "localagent/common/json_util.hpp"
#include "localagent/permissions/command_classifier.hpp"
#include "localagent/process/process.hpp"

namespace localagent {

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

  const auto classification = classify_command({"/bin/sh", "-c", *command});
  const auto risk = classification.primary_risk;
  if (ctx.permissions && ctx.permissions->check(risk, ctx.workspace_root) == Policy::Deny) {
    return ToolResult{false,
                      {},
                      {},
                      make_error(ErrorCategory::Permission, "denied", "shell command denied")};
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
