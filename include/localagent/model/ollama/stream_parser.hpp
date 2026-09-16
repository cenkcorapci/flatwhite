#pragma once

#include <localagent/model/model_types.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace localagent::ollama {

struct ParseError {
  std::string line;
  std::string message;
};

struct ParseResult {
  std::vector<ChatChunk> chunks;
  std::vector<ParseError> errors;
};

[[nodiscard]] std::optional<ChatChunk> parse_stream_line(std::string_view line);

[[nodiscard]] ParseResult parse_stream(std::string_view ndjson);

[[nodiscard]] ChatResponse aggregate_chunks(const std::vector<ChatChunk>& chunks);

}  // namespace localagent::ollama
