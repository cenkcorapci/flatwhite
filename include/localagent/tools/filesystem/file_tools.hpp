#pragma once

#include "localagent/tools/tool.hpp"

#include <memory>
#include <vector>

namespace localagent {

class ReadFileTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

class WriteFileTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

class ListDirectoryTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

class GlobTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

class GrepTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

[[nodiscard]] std::vector<std::unique_ptr<Tool>> make_file_tools();

}  // namespace localagent
