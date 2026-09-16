#pragma once

#include "localagent/tools/tool.hpp"

#include <memory>
#include <vector>

namespace localagent {

class GitStatusTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

class GitDiffTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

class GitLogTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

[[nodiscard]] std::vector<std::unique_ptr<Tool>> make_git_tools();

}  // namespace localagent
