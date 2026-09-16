#pragma once

#include "localagent/tools/tool.hpp"

#include <memory>

namespace localagent {

class ShellTool final : public Tool {
public:
  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;
};

[[nodiscard]] std::unique_ptr<Tool> make_shell_tool();

}  // namespace localagent
