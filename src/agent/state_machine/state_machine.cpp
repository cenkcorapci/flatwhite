#include "localagent/agent/state_machine/state_machine.hpp"

#include <unordered_set>

namespace localagent {

namespace {

using Transition = std::pair<AgentState, AgentState>;

bool allowed(AgentState from, AgentState to) {
  static const std::unordered_set<int> edges = {
      static_cast<int>(AgentState::Initializing) << 16 |
          static_cast<int>(AgentState::Understanding),
      static_cast<int>(AgentState::Initializing) << 16 |
          static_cast<int>(AgentState::BuildingContext),
      static_cast<int>(AgentState::Understanding) << 16 | static_cast<int>(AgentState::Planning),
      static_cast<int>(AgentState::Understanding) << 16 |
          static_cast<int>(AgentState::BuildingContext),
      static_cast<int>(AgentState::Planning) << 16 |
          static_cast<int>(AgentState::BuildingContext),
      static_cast<int>(AgentState::SelectingModel) << 16 |
          static_cast<int>(AgentState::BuildingContext),
      static_cast<int>(AgentState::BuildingContext) << 16 |
          static_cast<int>(AgentState::AwaitingModel),
      static_cast<int>(AgentState::AwaitingModel) << 16 |
          static_cast<int>(AgentState::ParsingResponse),
      static_cast<int>(AgentState::ParsingResponse) << 16 |
          static_cast<int>(AgentState::ExecutingTools),
      static_cast<int>(AgentState::ParsingResponse) << 16 |
          static_cast<int>(AgentState::Observing),
      static_cast<int>(AgentState::ParsingResponse) << 16 |
          static_cast<int>(AgentState::Verifying),
      static_cast<int>(AgentState::ParsingResponse) << 16 | static_cast<int>(AgentState::Completed),
      static_cast<int>(AgentState::AwaitingPermission) << 16 |
          static_cast<int>(AgentState::ExecutingTools),
      static_cast<int>(AgentState::AwaitingPermission) << 16 | static_cast<int>(AgentState::Failed),
      static_cast<int>(AgentState::ExecutingTools) << 16 | static_cast<int>(AgentState::Observing),
      static_cast<int>(AgentState::Observing) << 16 | static_cast<int>(AgentState::AwaitingModel),
      static_cast<int>(AgentState::Observing) << 16 | static_cast<int>(AgentState::Verifying),
      static_cast<int>(AgentState::Observing) << 16 | static_cast<int>(AgentState::Completed),
      static_cast<int>(AgentState::Verifying) << 16 | static_cast<int>(AgentState::Completed),
      static_cast<int>(AgentState::Verifying) << 16 | static_cast<int>(AgentState::Recovering),
      static_cast<int>(AgentState::Recovering) << 16 | static_cast<int>(AgentState::AwaitingModel),
      static_cast<int>(AgentState::Recovering) << 16 | static_cast<int>(AgentState::Failed),
  };
  const int key = static_cast<int>(from) << 16 | static_cast<int>(to);
  if (edges.contains(key)) {
    return true;
  }
  if (from == to) {
    return true;
  }
  if (to == AgentState::Failed || to == AgentState::Cancelled) {
    return from != AgentState::Completed;
  }
  return false;
}

}  // namespace

bool is_valid_transition(AgentState from, AgentState to) { return allowed(from, to); }

std::optional<AgentState> default_next_state(AgentState current) {
  switch (current) {
    case AgentState::Initializing:
      return AgentState::BuildingContext;
    case AgentState::BuildingContext:
      return AgentState::AwaitingModel;
    case AgentState::AwaitingModel:
      return AgentState::ParsingResponse;
    case AgentState::ParsingResponse:
      return AgentState::Observing;
    default:
      return std::nullopt;
  }
}

bool is_terminal_state(AgentState state) {
  return state == AgentState::Completed || state == AgentState::Failed ||
         state == AgentState::Cancelled;
}

StateMachine::StateMachine(AgentState initial) : state_(initial) {}

bool StateMachine::transition(AgentState to) {
  if (!is_valid_transition(state_, to)) {
    last_error_ = "invalid transition from " + to_string(state_) + " to " + to_string(to);
    return false;
  }
  state_ = to;
  last_error_.clear();
  return true;
}

}  // namespace localagent
