#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace localagent {

enum class AgentMode {
  Agent,
  Plan,
  Ask,
  Debug,
  Review,
  Chat,
  Data,
  Research
};

[[nodiscard]] std::string to_string(AgentMode mode);
[[nodiscard]] std::optional<AgentMode> parse_mode(std::string_view text);

enum class AgentState {
  Initializing,
  Understanding,
  Planning,
  SelectingModel,
  BuildingContext,
  AwaitingModel,
  ParsingResponse,
  AwaitingPermission,
  ExecutingTools,
  Observing,
  Verifying,
  Recovering,
  Completed,
  Failed,
  Cancelled
};

[[nodiscard]] std::string to_string(AgentState state);

enum class TaskCategory {
  Conversation,
  SimpleCodeEdit,
  LargeCodeEdit,
  Debugging,
  Architecture,
  CodeReview,
  Summarization,
  DataQuery,
  DataAnalysis,
  Research,
  Planning,
  ToolSelection,
  ContextCompression
};

[[nodiscard]] std::string to_string(TaskCategory category);
[[nodiscard]] TaskCategory classify_task(std::string_view goal, AgentMode mode);

struct Budget {
  uint32_t max_turns{40};
  uint32_t max_tool_calls{100};
  uint32_t max_subagents{2};
  uint64_t max_input_tokens{128000};
  uint64_t max_output_tokens{16000};
  std::chrono::seconds max_duration{std::chrono::seconds{1800}};

  uint32_t used_turns{0};
  uint32_t used_tool_calls{0};
  uint32_t used_subagents{0};
  uint64_t used_input_tokens{0};
  uint64_t used_output_tokens{0};
};

[[nodiscard]] Budget default_budget_for(AgentMode mode);
[[nodiscard]] bool budget_exhausted(const Budget& budget);
[[nodiscard]] std::string budget_warning_json(const Budget& budget);

}  // namespace localagent
