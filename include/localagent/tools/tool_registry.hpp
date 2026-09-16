#pragma once

#include "localagent/tools/tool.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace localagent {

class ToolRegistry {
public:
  void register_tool(std::unique_ptr<Tool> tool);
  void register_defaults(const ToolContext& ctx);

  [[nodiscard]] std::optional<ToolResult> dispatch(const ToolRequest& request,
                                                     const ToolContext& ctx) const;

  [[nodiscard]] const Tool* find(std::string_view name) const;
  [[nodiscard]] std::vector<ToolDescriptor> list_descriptors() const;
  [[nodiscard]] std::size_t size() const { return tools_.size(); }

private:
  std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

}  // namespace localagent
