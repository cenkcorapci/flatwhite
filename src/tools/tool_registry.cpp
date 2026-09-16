#include "localagent/tools/tool_registry.hpp"

#include <algorithm>

#include "localagent/tools/filesystem/file_tools.hpp"
#include "localagent/tools/git/git_tools.hpp"
#include "localagent/tools/shell/shell_tool.hpp"

namespace localagent {

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
  const auto name = tool->descriptor().name;
  tools_[name] = std::move(tool);
}

void ToolRegistry::register_defaults(const ToolContext& /*ctx*/) {
  for (auto& tool : make_file_tools()) {
    register_tool(std::move(tool));
  }
  register_tool(make_shell_tool());
  for (auto& tool : make_git_tools()) {
    register_tool(std::move(tool));
  }
}

std::optional<ToolResult> ToolRegistry::dispatch(const ToolRequest& request,
                                                   const ToolContext& ctx) const {
  const Tool* tool = find(request.tool_name);
  if (!tool) {
    return ToolResult{false,
                      {},
                      {},
                      make_error(ErrorCategory::Tool, "unknown_tool",
                                 "unknown tool: " + request.tool_name)};
  }
  return tool->execute(request, ctx);
}

const Tool* ToolRegistry::find(std::string_view name) const {
  const auto it = tools_.find(std::string(name));
  if (it == tools_.end()) {
    return nullptr;
  }
  return it->second.get();
}

std::vector<ToolDescriptor> ToolRegistry::list_descriptors() const {
  std::vector<ToolDescriptor> out;
  out.reserve(tools_.size());
  for (const auto& [_, tool] : tools_) {
    out.push_back(tool->descriptor());
  }
  std::sort(out.begin(), out.end(),
            [](const ToolDescriptor& a, const ToolDescriptor& b) { return a.name < b.name; });
  return out;
}

}  // namespace localagent
