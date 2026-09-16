#pragma once

#include <localagent/common/strong_id.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace localagent {

struct ModelTag {};
using ModelId = StrongId<ModelTag>;

enum class MessageRole { System, User, Assistant, Tool };

[[nodiscard]] std::string to_string(MessageRole role);
[[nodiscard]] std::optional<MessageRole> parse_message_role(std::string_view text);

struct Message {
  MessageRole role{MessageRole::User};
  std::string content;
  std::optional<std::string> name;
  std::optional<std::string> tool_call_id;
};

struct ToolCall {
  std::string id;
  std::string name;
  std::string arguments_json;
};

struct ToolDefinition {
  std::string name;
  std::string description;
  std::string parameters_json;
};

enum class Capability : uint32_t {
  Chat = 1u << 0,
  Embedding = 1u << 1,
  ToolUse = 1u << 2,
  Vision = 1u << 3,
  Code = 1u << 4,
  Reasoning = 1u << 5,
};

using Capabilities = uint32_t;

[[nodiscard]] constexpr Capabilities capability(Capability c) noexcept {
  return static_cast<Capabilities>(c);
}

[[nodiscard]] constexpr bool has_capability(Capabilities set, Capability c) noexcept {
  return (set & capability(c)) != 0u;
}

[[nodiscard]] constexpr Capabilities operator|(Capabilities set, Capability c) noexcept {
  return set | capability(c);
}

struct SamplingConfig {
  float temperature{0.7f};
  float top_p{0.9f};
  int max_tokens{4096};
  std::optional<std::vector<std::string>> stop;
};

struct TokenUsage {
  uint32_t prompt_tokens{0};
  uint32_t completion_tokens{0};
  uint32_t total_tokens{0};
};

struct ChatRequest {
  ModelId model;
  std::vector<Message> messages;
  std::vector<ToolDefinition> tools;
  SamplingConfig sampling{};
  bool stream{false};
};

struct ChatChunk {
  std::string content_delta;
  std::vector<ToolCall> tool_calls;
  bool done{false};
  std::optional<std::string> finish_reason;
  std::optional<TokenUsage> usage;
  std::optional<std::string> model;
};

struct ChatResponse {
  Message message;
  std::vector<ToolCall> tool_calls;
  std::optional<TokenUsage> usage;
  std::string model;
};

struct EmbeddingRequest {
  ModelId model;
  std::string text;
};

struct EmbeddingResult {
  std::vector<float> embedding;
  std::string model;
};

struct ModelInfo {
  ModelId id;
  std::string display_name;
  Capabilities capabilities{};
  uint32_t context_length{8192};
  float quality_score{1.0f};
  float speed_score{1.0f};
  uint32_t memory_mb{4096};
  float cost_score{1.0f};
};

}  // namespace localagent
