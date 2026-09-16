#include "localagent/tools/git/git_tools.hpp"

#include "localagent/common/json_util.hpp"
#include "localagent/process/process.hpp"

namespace localagent {

namespace {

ToolResult run_git(const std::vector<std::string>& extra_argv, const ToolContext& ctx) {
  std::vector<std::string> argv{"git"};
  argv.insert(argv.end(), extra_argv.begin(), extra_argv.end());
  ctx.cancellation.throw_if_cancelled();
  const auto result = run_process(argv, ctx.workspace_root, ctx.cancellation);
  nlohmann::json meta{{"exit_code", result.exit_code}};
  const bool success = result.exit_code == 0;
  std::string content = result.stdout_str;
  if (!result.stderr_str.empty()) {
    content += "\n" + result.stderr_str;
  }
  return ToolResult{success, std::move(content), std::move(meta), std::nullopt};
}

}  // namespace

ToolDescriptor GitStatusTool::descriptor() const {
  return ToolDescriptor{"git_status", "Show git working tree status.", {{"type", "object"}}};
}

ToolResult GitStatusTool::execute(const ToolRequest& /*request*/, const ToolContext& ctx) const {
  return run_git({"status", "--short", "--branch"}, ctx);
}

ToolDescriptor GitDiffTool::descriptor() const {
  return ToolDescriptor{
      "git_diff",
      "Show git diff.",
      {{"type", "object"},
       {"properties", {{"staged", {{"type", "boolean"}, {"default", false}}}}}}};
}

ToolResult GitDiffTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const bool staged = json_bool(request.arguments, "staged").value_or(false);
  if (staged) {
    return run_git({"diff", "--staged"}, ctx);
  }
  return run_git({"diff"}, ctx);
}

ToolDescriptor GitLogTool::descriptor() const {
  return ToolDescriptor{
      "git_log",
      "Show recent git commits.",
      {{"type", "object"},
       {"properties", {{"limit", {{"type", "integer"}, {"default", 10}}}}}}};
}

ToolResult GitLogTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const int limit = json_int(request.arguments, "limit").value_or(10);
  return run_git({"log", "-n", std::to_string(limit), "--oneline"}, ctx);
}

std::vector<std::unique_ptr<Tool>> make_git_tools() {
  std::vector<std::unique_ptr<Tool>> tools;
  tools.push_back(std::make_unique<GitStatusTool>());
  tools.push_back(std::make_unique<GitDiffTool>());
  tools.push_back(std::make_unique<GitLogTool>());
  return tools;
}

}  // namespace localagent
