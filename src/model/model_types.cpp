#include <localagent/model/model_types.hpp>

#include <algorithm>
#include <cctype>

namespace localagent {

namespace {

std::string lower(std::string_view text) {
  std::string out(text);
  std::ranges::transform(out, out.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

}  // namespace

std::string to_string(MessageRole role) {
  switch (role) {
    case MessageRole::System:
      return "system";
    case MessageRole::User:
      return "user";
    case MessageRole::Assistant:
      return "assistant";
    case MessageRole::Tool:
      return "tool";
  }
  return "user";
}

std::optional<MessageRole> parse_message_role(std::string_view text) {
  const auto normalized = lower(text);
  if (normalized == "system") {
    return MessageRole::System;
  }
  if (normalized == "user") {
    return MessageRole::User;
  }
  if (normalized == "assistant") {
    return MessageRole::Assistant;
  }
  if (normalized == "tool") {
    return MessageRole::Tool;
  }
  return std::nullopt;
}

}  // namespace localagent
