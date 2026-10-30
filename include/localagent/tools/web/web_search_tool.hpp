#pragma once

#include "localagent/tools/tool.hpp"

#include <memory>
#include <string>

namespace localagent {

struct WebSearchResponse {
  long status_code{0};
  std::string body;
  std::string error;
};

class WebSearchTransport {
public:
  virtual ~WebSearchTransport() = default;
  [[nodiscard]] virtual WebSearchResponse get(const std::string& url) = 0;
};

class CurlWebSearchTransport final : public WebSearchTransport {
public:
  [[nodiscard]] WebSearchResponse get(const std::string& url) override;
};

[[nodiscard]] std::string build_web_search_url(std::string_view endpoint, std::string_view query);

class WebSearchTool final : public Tool {
public:
  explicit WebSearchTool(std::string endpoint = {},
                         std::shared_ptr<WebSearchTransport> transport = nullptr);

  [[nodiscard]] ToolDescriptor descriptor() const override;
  [[nodiscard]] ToolResult execute(const ToolRequest& request,
                                   const ToolContext& ctx) const override;

private:
  std::string endpoint_;
  std::shared_ptr<WebSearchTransport> transport_;
};

[[nodiscard]] std::unique_ptr<Tool> make_web_search_tool(std::string endpoint = {});

}  // namespace localagent
