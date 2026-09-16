#include <catch2/catch_test_macros.hpp>

#include "localagent/agent/types.hpp"
#include "localagent/common/json_util.hpp"

TEST_CASE("AgentMode to_string and parse_mode roundtrip", "[budget]") {
  REQUIRE(localagent::to_string(localagent::AgentMode::Agent) == "agent");
  REQUIRE(localagent::parse_mode("plan") == localagent::AgentMode::Plan);
  REQUIRE_FALSE(localagent::parse_mode("invalid-mode").has_value());
}

TEST_CASE("classify_task uses mode and goal heuristics", "[budget]") {
  REQUIRE(localagent::classify_task("explain this function", localagent::AgentMode::Ask) ==
          localagent::TaskCategory::Conversation);
  REQUIRE(localagent::classify_task("implement feature", localagent::AgentMode::Agent) ==
          localagent::TaskCategory::LargeCodeEdit);
  REQUIRE(localagent::classify_task("analyze dataset", localagent::AgentMode::Data) ==
          localagent::TaskCategory::DataAnalysis);
  REQUIRE(localagent::classify_task("anything", localagent::AgentMode::Plan) ==
          localagent::TaskCategory::Planning);
}

TEST_CASE("default_budget_for varies by mode", "[budget]") {
  const auto chat = localagent::default_budget_for(localagent::AgentMode::Chat);
  const auto agent = localagent::default_budget_for(localagent::AgentMode::Agent);
  const auto research = localagent::default_budget_for(localagent::AgentMode::Research);

  REQUIRE(chat.max_turns == 20);
  REQUIRE(chat.max_tool_calls == 0);
  REQUIRE(agent.max_turns == 40);
  REQUIRE(agent.max_tool_calls == 100);
  REQUIRE(research.max_turns == 100);
  REQUIRE(research.max_subagents == 4);
}

TEST_CASE("budget_exhausted detects limits", "[budget]") {
  auto budget = localagent::default_budget_for(localagent::AgentMode::Agent);
  REQUIRE_FALSE(localagent::budget_exhausted(budget));

  budget.used_turns = budget.max_turns;
  REQUIRE(localagent::budget_exhausted(budget));

  budget = localagent::default_budget_for(localagent::AgentMode::Agent);
  budget.used_tool_calls = budget.max_tool_calls;
  REQUIRE(localagent::budget_exhausted(budget));
}

TEST_CASE("budget_warning_json reports remaining resources", "[budget]") {
  auto budget = localagent::default_budget_for(localagent::AgentMode::Agent);
  budget.used_turns = 10;
  budget.used_tool_calls = 25;

  const auto json = localagent::parse_or_throw(localagent::budget_warning_json(budget));
  REQUIRE(json.at("budget").at("remaining_turns") == 30);
  REQUIRE(json.at("budget").at("remaining_tool_calls") == 75);
}
