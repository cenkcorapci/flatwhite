#include <localagent/model/ollama/stream_parser.hpp>

#include <nlohmann/json.hpp>

#include <sstream>

namespace localagent::ollama {
namespace {

using json = nlohmann::json;

std::optional<ToolCall> parse_tool_call(const json& node) {
  if (!node.is_object()) {
    return std::nullopt;
  }

  ToolCall call;
  if (node.contains("id") && node["id"].is_string()) {
    call.id = node["id"].get<std::string>();
  }
  if (node.contains("function") && node["function"].is_object()) {
    const auto& fn = node["function"];
    if (fn.contains("name") && fn["name"].is_string()) {
      call.name = fn["name"].get<std::string>();
    }
    if (fn.contains("arguments")) {
      if (fn["arguments"].is_string()) {
        call.arguments_json = fn["arguments"].get<std::string>();
      } else {
        call.arguments_json = fn["arguments"].dump();
      }
    }
  } else {
    if (node.contains("name") && node["name"].is_string()) {
      call.name = node["name"].get<std::string>();
    }
    if (node.contains("arguments")) {
      if (node["arguments"].is_string()) {
        call.arguments_json = node["arguments"].get<std::string>();
      } else {
        call.arguments_json = node["arguments"].dump();
      }
    }
  }

  if (call.name.empty()) {
    return std::nullopt;
  }
  return call;
}

std::vector<ToolCall> parse_tool_calls(const json& message) {
  std::vector<ToolCall> calls;
  if (!message.contains("tool_calls") || !message["tool_calls"].is_array()) {
    return calls;
  }

  for (const auto& node : message["tool_calls"]) {
    if (auto call = parse_tool_call(node)) {
      calls.push_back(std::move(*call));
    }
  }
  return calls;
}

std::optional<TokenUsage> parse_usage(const json& root) {
  if (!root.contains("prompt_eval_count") && !root.contains("eval_count")) {
    return std::nullopt;
  }

  TokenUsage usage;
  if (root.contains("prompt_eval_count") && root["prompt_eval_count"].is_number_unsigned()) {
    usage.prompt_tokens = root["prompt_eval_count"].get<uint32_t>();
  }
  if (root.contains("eval_count") && root["eval_count"].is_number_unsigned()) {
    usage.completion_tokens = root["eval_count"].get<uint32_t>();
  }
  usage.total_tokens = usage.prompt_tokens + usage.completion_tokens;
  return usage;
}

std::optional<ChatChunk> parse_json_chunk(const json& root) {
  ChatChunk chunk;

  if (root.contains("model") && root["model"].is_string()) {
    chunk.model = root["model"].get<std::string>();
  }

  if (root.contains("message") && root["message"].is_object()) {
    const auto& message = root["message"];
    if (message.contains("content") && message["content"].is_string()) {
      chunk.content_delta = message["content"].get<std::string>();
    }
    chunk.tool_calls = parse_tool_calls(message);
  }

  if (root.contains("done") && root["done"].is_boolean()) {
    chunk.done = root["done"].get<bool>();
  }

  if (chunk.done) {
    chunk.finish_reason = "stop";
    if (root.contains("done_reason") && root["done_reason"].is_string()) {
      chunk.finish_reason = root["done_reason"].get<std::string>();
    }
    if (auto usage = parse_usage(root)) {
      chunk.usage = std::move(usage);
    }
  } else if (auto usage = parse_usage(root)) {
    chunk.usage = std::move(usage);
  }

  return chunk;
}

}  // namespace

std::optional<ChatChunk> parse_stream_line(std::string_view line) {
  if (line.empty()) {
    return std::nullopt;
  }

  while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
    line.remove_prefix(1);
  }
  if (line.empty()) {
    return std::nullopt;
  }

  try {
    const json root = json::parse(line.begin(), line.end());
    if (!root.is_object()) {
      return std::nullopt;
    }
    return parse_json_chunk(root);
  } catch (const json::parse_error&) {
    return std::nullopt;
  }
}

ParseResult parse_stream(std::string_view ndjson) {
  ParseResult result;
  std::istringstream input{std::string{ndjson}};
  std::string line;

  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    if (auto chunk = parse_stream_line(line)) {
      result.chunks.push_back(std::move(*chunk));
    } else {
      result.errors.push_back(
          ParseError{.line = line, .message = "invalid or truncated JSON line"});
    }
  }

  return result;
}

ChatResponse aggregate_chunks(const std::vector<ChatChunk>& chunks) {
  ChatResponse response;
  response.message.role = MessageRole::Assistant;

  for (const auto& chunk : chunks) {
    response.message.content += chunk.content_delta;
    if (chunk.model) {
      response.model = *chunk.model;
    }
    for (const auto& call : chunk.tool_calls) {
      response.tool_calls.push_back(call);
    }
    if (chunk.usage) {
      response.usage = chunk.usage;
    }
  }

  return response;
}

}  // namespace localagent::ollama
