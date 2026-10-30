#include "localagent/tools/web/web_search_tool.hpp"

#include "localagent/common/json_util.hpp"

#include <curl/curl.h>

#include <cctype>
#include <utility>

namespace localagent {
namespace {

constexpr std::string_view kDefaultEndpoint = "https://html.duckduckgo.com/html/?q={query}";

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  const auto total = size * nmemb;
  static_cast<std::string*>(userdata)->append(ptr, total);
  return total;
}

ToolResult fail(ErrorCategory category, std::string code, std::string message, bool retryable = false) {
  return ToolResult{false, {}, {}, make_error(category, std::move(code), std::move(message), retryable)};
}

std::string percent_encode(std::string_view value) {
  static constexpr char hex[] = "0123456789ABCDEF";
  std::string encoded;
  encoded.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~') {
      encoded.push_back(static_cast<char>(ch));
    } else {
      encoded.push_back('%');
      encoded.push_back(hex[ch >> 4]);
      encoded.push_back(hex[ch & 0x0F]);
    }
  }
  return encoded;
}

}  // namespace

WebSearchResponse CurlWebSearchTransport::get(const std::string& url) {
  CURL* handle = curl_easy_init();
  if (!handle) {
    return {.error = "curl_easy_init failed"};
  }

  WebSearchResponse response;
  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(handle, CURLOPT_USERAGENT, "localagent-web-search/0.1");

  const auto code = curl_easy_perform(handle);
  if (code != CURLE_OK) {
    response.error = curl_easy_strerror(code);
  } else {
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
  }
  curl_easy_cleanup(handle);
  return response;
}

std::string build_web_search_url(std::string_view endpoint, std::string_view query) {
  const auto encoded = percent_encode(query);
  std::string url = endpoint.empty() ? std::string{kDefaultEndpoint} : std::string{endpoint};
  constexpr std::string_view placeholder = "{query}";
  std::size_t offset = 0;
  bool replaced = false;
  while ((offset = url.find(placeholder, offset)) != std::string::npos) {
    url.replace(offset, placeholder.size(), encoded);
    offset += encoded.size();
    replaced = true;
  }
  if (!replaced) {
    url += url.find('?') == std::string::npos ? "?q=" : "&q=";
    url += encoded;
  }
  return url;
}

WebSearchTool::WebSearchTool(std::string endpoint, std::shared_ptr<WebSearchTransport> transport)
    : endpoint_(endpoint.empty() ? std::string{kDefaultEndpoint} : std::move(endpoint)),
      transport_(transport ? std::move(transport) : std::make_shared<CurlWebSearchTransport>()) {}

ToolDescriptor WebSearchTool::descriptor() const {
  return ToolDescriptor{
      "web_search",
      "Search the web through the configured provider and return its response body.",
      {{"type", "object"},
       {"properties",
        {{"query", {{"type", "string"}, {"minLength", 1}, {"description", "Web search query"}}}}},
       {"required", nlohmann::json::array({"query"})},
       {"additionalProperties", false}}};
}

ToolResult WebSearchTool::execute(const ToolRequest& request, const ToolContext& ctx) const {
  const auto query = json_string(request.arguments, "query");
  if (!query || query->empty()) {
    return fail(ErrorCategory::Tool, "missing_query", "web_search requires a non-empty query");
  }
  ctx.cancellation.throw_if_cancelled();
  if (ctx.permissions) {
    const auto decision = ctx.permissions->evaluate(RiskLevel::Network);
    if (!ctx.permissions->allows(decision.policy)) {
      return fail(ErrorCategory::Permission, "denied",
                  "web search denied: " + decision.reason);
    }
  }

  const auto response = transport_->get(build_web_search_url(endpoint_, *query));
  if (!response.error.empty()) {
    return fail(ErrorCategory::Network, "web_search_failed",
                "web search request failed: " + response.error, true);
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    return fail(ErrorCategory::Network, "web_search_http_error",
                "web search returned HTTP " + std::to_string(response.status_code), true);
  }
  return ToolResult{true, response.body, {{"status_code", response.status_code}}, std::nullopt};
}

std::unique_ptr<Tool> make_web_search_tool(std::string endpoint) {
  return std::make_unique<WebSearchTool>(std::move(endpoint));
}

}  // namespace localagent
