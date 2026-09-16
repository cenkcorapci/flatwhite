#pragma once

#include "localagent/agent/types.hpp"

#include <optional>
#include <string>

namespace localagent {

[[nodiscard]] bool is_valid_transition(AgentState from, AgentState to);
[[nodiscard]] std::optional<AgentState> default_next_state(AgentState current);
[[nodiscard]] bool is_terminal_state(AgentState state);

class StateMachine {
public:
  explicit StateMachine(AgentState initial = AgentState::Initializing);

  [[nodiscard]] AgentState state() const { return state_; }
  [[nodiscard]] bool transition(AgentState to);
  [[nodiscard]] std::string last_error() const { return last_error_; }

private:
  AgentState state_;
  std::string last_error_;
};

}  // namespace localagent
