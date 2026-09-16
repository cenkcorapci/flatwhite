#include <localagent/model/ollama/stream_parser.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace localagent;
using namespace localagent::ollama;

TEST_CASE("parse_stream_line handles valid content chunk", "[stream_parser]") {
  const auto chunk = parse_stream_line(
      R"({"model":"llama3","message":{"role":"assistant","content":"Hello"},"done":false})");
  REQUIRE(chunk.has_value());
  CHECK(chunk->content_delta == "Hello");
  CHECK_FALSE(chunk->done);
  CHECK(chunk->model == "llama3");
  CHECK(chunk->tool_calls.empty());
}

TEST_CASE("parse_stream_line handles done chunk with usage", "[stream_parser]") {
  const auto chunk = parse_stream_line(
      R"({"model":"llama3","message":{"role":"assistant","content":""},"done":true,"prompt_eval_count":12,"eval_count":4})");
  REQUIRE(chunk.has_value());
  CHECK(chunk->done);
  REQUIRE(chunk->usage.has_value());
  CHECK(chunk->usage->prompt_tokens == 12);
  CHECK(chunk->usage->completion_tokens == 4);
  CHECK(chunk->usage->total_tokens == 16);
}

TEST_CASE("parse_stream_line detects tool calls", "[stream_parser]") {
  const auto chunk = parse_stream_line(
      R"({"message":{"role":"assistant","content":"","tool_calls":[{"id":"call_1","function":{"name":"read_file","arguments":{"path":"main.cpp"}}}]},"done":true})");
  REQUIRE(chunk.has_value());
  REQUIRE(chunk->tool_calls.size() == 1);
  CHECK(chunk->tool_calls[0].id == "call_1");
  CHECK(chunk->tool_calls[0].name == "read_file");
  CHECK(chunk->tool_calls[0].arguments_json.find("main.cpp") != std::string::npos);
}

TEST_CASE("parse_stream_line accepts alternate tool call shape", "[stream_parser]") {
  const auto chunk = parse_stream_line(
      R"({"message":{"tool_calls":[{"name":"grep","arguments":"{\"pattern\":\"foo\"}"}]},"done":false})");
  REQUIRE(chunk.has_value());
  REQUIRE(chunk->tool_calls.size() == 1);
  CHECK(chunk->tool_calls[0].name == "grep");
  CHECK(chunk->tool_calls[0].arguments_json == R"({"pattern":"foo"})");
}

TEST_CASE("parse_stream_line rejects invalid JSON", "[stream_parser]") {
  CHECK_FALSE(parse_stream_line("{not json"));
  CHECK_FALSE(parse_stream_line("{\"message\":"));
}

TEST_CASE("parse_stream_line ignores empty and whitespace lines", "[stream_parser]") {
  CHECK_FALSE(parse_stream_line(""));
  CHECK_FALSE(parse_stream_line("   "));
}

TEST_CASE("parse_stream handles multi-line NDJSON stream", "[stream_parser]") {
  const std::string stream = R"({"message":{"content":"Hel"},"done":false}
{"message":{"content":"lo"},"done":false}
{"message":{"content":"!"},"done":true,"eval_count":3})";

  const auto result = parse_stream(stream);
  CHECK(result.errors.empty());
  REQUIRE(result.chunks.size() == 3);
  CHECK(result.chunks[0].content_delta == "Hel");
  CHECK(result.chunks[2].done);
}

TEST_CASE("parse_stream records malformed lines", "[stream_parser]") {
  const std::string stream = R"({"message":{"content":"ok"},"done":false}
{broken}
{"message":{"content":"after"},"done":true})";

  const auto result = parse_stream(stream);
  REQUIRE(result.errors.size() == 1);
  REQUIRE(result.chunks.size() == 2);
  CHECK(result.chunks[1].content_delta == "after");
}

TEST_CASE("aggregate_chunks merges content and tool calls", "[stream_parser]") {
  std::vector<ChatChunk> chunks;
  chunks.push_back(ChatChunk{.content_delta = "Run ", .model = "m1"});
  chunks.push_back(ChatChunk{.content_delta = "tool"});
  chunks.push_back(ChatChunk{
      .tool_calls = {ToolCall{.id = "1", .name = "shell", .arguments_json = "{}"}},
      .done = true,
      .usage = TokenUsage{.prompt_tokens = 1, .completion_tokens = 2, .total_tokens = 3},
  });

  const auto response = aggregate_chunks(chunks);
  CHECK(response.message.content == "Run tool");
  CHECK(response.model == "m1");
  REQUIRE(response.tool_calls.size() == 1);
  CHECK(response.tool_calls[0].name == "shell");
  REQUIRE(response.usage.has_value());
  CHECK(response.usage->total_tokens == 3);
}

TEST_CASE("parse_stream handles empty input", "[stream_parser]") {
  const auto result = parse_stream("");
  CHECK(result.chunks.empty());
  CHECK(result.errors.empty());
}

TEST_CASE("parse_stream handles truncated final line", "[stream_parser]") {
  const auto result = parse_stream(R"({"message":{"content":"partial")");
  CHECK(result.chunks.empty());
  REQUIRE(result.errors.size() == 1);
}
