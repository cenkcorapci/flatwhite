#include <localagent/agent/types.hpp>

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

bool contains_any(std::string_view haystack, std::initializer_list<std::string_view> needles) {
  const auto normalized = lower(haystack);
  for (const auto needle : needles) {
    if (normalized.find(std::string{lower(needle)}) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace

std::string to_string(AgentMode mode) {
  switch (mode) {
    case AgentMode::Agent:
      return "agent";
    case AgentMode::Plan:
      return "plan";
    case AgentMode::Ask:
      return "ask";
    case AgentMode::Debug:
      return "debug";
    case AgentMode::Review:
      return "review";
    case AgentMode::Chat:
      return "chat";
    case AgentMode::Data:
      return "data";
    case AgentMode::Research:
      return "research";
  }
  return "agent";
}

std::optional<AgentMode> parse_mode(std::string_view text) {
  const auto normalized = lower(text);
  if (normalized == "agent") {
    return AgentMode::Agent;
  }
  if (normalized == "plan") {
    return AgentMode::Plan;
  }
  if (normalized == "ask") {
    return AgentMode::Ask;
  }
  if (normalized == "debug") {
    return AgentMode::Debug;
  }
  if (normalized == "review") {
    return AgentMode::Review;
  }
  if (normalized == "chat") {
    return AgentMode::Chat;
  }
  if (normalized == "data") {
    return AgentMode::Data;
  }
  if (normalized == "research") {
    return AgentMode::Research;
  }
  return std::nullopt;
}

std::string to_string(AgentState state) {
  switch (state) {
    case AgentState::Initializing:
      return "initializing";
    case AgentState::Understanding:
      return "understanding";
    case AgentState::Planning:
      return "planning";
    case AgentState::SelectingModel:
      return "selecting_model";
    case AgentState::BuildingContext:
      return "building_context";
    case AgentState::AwaitingModel:
      return "awaiting_model";
    case AgentState::ParsingResponse:
      return "parsing_response";
    case AgentState::AwaitingPermission:
      return "awaiting_permission";
    case AgentState::ExecutingTools:
      return "executing_tools";
    case AgentState::Observing:
      return "observing";
    case AgentState::Verifying:
      return "verifying";
    case AgentState::Recovering:
      return "recovering";
    case AgentState::Completed:
      return "completed";
    case AgentState::Failed:
      return "failed";
    case AgentState::Cancelled:
      return "cancelled";
  }
  return "initializing";
}

std::string to_string(TaskCategory category) {
  switch (category) {
    case TaskCategory::Conversation:
      return "conversation";
    case TaskCategory::SimpleCodeEdit:
      return "simple_code_edit";
    case TaskCategory::LargeCodeEdit:
      return "large_code_edit";
    case TaskCategory::Debugging:
      return "debugging";
    case TaskCategory::Architecture:
      return "architecture";
    case TaskCategory::CodeReview:
      return "code_review";
    case TaskCategory::Summarization:
      return "summarization";
    case TaskCategory::DataQuery:
      return "data_query";
    case TaskCategory::DataAnalysis:
      return "data_analysis";
    case TaskCategory::Research:
      return "research";
    case TaskCategory::Planning:
      return "planning";
    case TaskCategory::ToolSelection:
      return "tool_selection";
    case TaskCategory::ContextCompression:
      return "context_compression";
  }
  return "conversation";
}

TaskCategory classify_task(std::string_view goal, AgentMode mode) {
  if (mode == AgentMode::Debug) {
    return TaskCategory::Debugging;
  }
  if (mode == AgentMode::Plan) {
    return TaskCategory::Planning;
  }
  if (mode == AgentMode::Review) {
    return TaskCategory::CodeReview;
  }
  if (mode == AgentMode::Research) {
    return TaskCategory::Research;
  }
  if (mode == AgentMode::Data) {
    return contains_any(goal, {"analyze", "analysis", "chart", "plot"})
               ? TaskCategory::DataAnalysis
               : TaskCategory::DataQuery;
  }
  if (mode == AgentMode::Ask) {
    return TaskCategory::Conversation;
  }

  if (contains_any(goal, {"architect", "architecture", "design system", "refactor entire"})) {
    return TaskCategory::Architecture;
  }
  if (contains_any(goal, {"debug", "fix crash", "stack trace", "regression"})) {
    return TaskCategory::Debugging;
  }
  if (contains_any(goal, {"review", "audit", "security review"})) {
    return TaskCategory::CodeReview;
  }
  if (contains_any(goal, {"summarize", "summary", "tl;dr", "compress context"})) {
    return contains_any(goal, {"context", "compress"}) ? TaskCategory::ContextCompression
                                                       : TaskCategory::Summarization;
  }
  if (contains_any(goal, {"rename", "typo", "small change", "one line"})) {
    return TaskCategory::SimpleCodeEdit;
  }
  if (contains_any(goal, {"implement", "rewrite", "migrate", "across files", "large refactor"})) {
    return TaskCategory::LargeCodeEdit;
  }
  if (contains_any(goal, {"research", "investigate", "compare options"})) {
    return TaskCategory::Research;
  }

  return TaskCategory::Conversation;
}

Budget default_budget_for(AgentMode mode) {
  Budget budget;
  switch (mode) {
    case AgentMode::Chat:
      budget.max_turns = 20;
      budget.max_tool_calls = 0;
      budget.max_subagents = 0;
      break;
    case AgentMode::Ask:
    case AgentMode::Plan:
    case AgentMode::Review:
      budget.max_turns = 20;
      budget.max_tool_calls = 40;
      budget.max_subagents = 0;
      break;
    case AgentMode::Research:
      budget.max_turns = 100;
      budget.max_tool_calls = 200;
      budget.max_subagents = 4;
      budget.max_duration = std::chrono::seconds{7200};
      break;
    case AgentMode::Data:
      budget.max_turns = 40;
      budget.max_tool_calls = 80;
      break;
    case AgentMode::Debug:
    case AgentMode::Agent:
    default:
      budget.max_turns = 40;
      budget.max_tool_calls = 100;
      budget.max_subagents = 2;
      break;
  }
  return budget;
}

bool budget_exhausted(const Budget& budget) {
  return budget.used_turns >= budget.max_turns ||
         budget.used_tool_calls >= budget.max_tool_calls ||
         budget.used_subagents >= budget.max_subagents ||
         budget.used_input_tokens >= budget.max_input_tokens ||
         budget.used_output_tokens >= budget.max_output_tokens;
}

std::string budget_warning_json(const Budget& budget) {
  const auto remaining_turns =
      budget.used_turns >= budget.max_turns ? 0u : (budget.max_turns - budget.used_turns);
  const auto remaining_tools = budget.used_tool_calls >= budget.max_tool_calls
                                   ? 0u
                                   : (budget.max_tool_calls - budget.used_tool_calls);
  return std::string{"{\"budget\":{\"remaining_turns\":"} + std::to_string(remaining_turns) +
         ",\"remaining_tool_calls\":" + std::to_string(remaining_tools) + "}}";
}

}  // namespace localagent
