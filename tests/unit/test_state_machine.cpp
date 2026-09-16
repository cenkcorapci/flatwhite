#include "localagent/agent/state_machine/state_machine.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

TEST_CASE("StateMachine validates transitions", "[state_machine]") {
  CHECK(is_valid_transition(AgentState::Initializing, AgentState::BuildingContext));
  CHECK_FALSE(is_valid_transition(AgentState::Completed, AgentState::AwaitingModel));

  StateMachine sm;
  CHECK(sm.transition(AgentState::BuildingContext));
  CHECK(sm.state() == AgentState::BuildingContext);
  CHECK_FALSE(sm.transition(AgentState::Completed));
  CHECK_FALSE(sm.last_error().empty());
}

TEST_CASE("terminal states are detected", "[state_machine]") {
  CHECK(is_terminal_state(AgentState::Completed));
  CHECK(is_terminal_state(AgentState::Failed));
  CHECK_FALSE(is_terminal_state(AgentState::Observing));
}
