#pragma once

#include "localagent/agent/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace localagent {

enum class OutputFormat { Text, Json };

enum class CommandKind {
  Help,
  Version,
  Chat,
  Agent,
  Plan,
  Ask,
  Debug,
  Review,
  Data,
  Research,
  Prompt,
  SessionsList,
  SessionsResume,
  ModelsList
};

struct Command {
  CommandKind kind{CommandKind::Help};
  AgentMode mode{AgentMode::Chat};
  std::string prompt;
  OutputFormat output_format{OutputFormat::Text};
  bool continue_session{false};
  std::optional<std::string> session_id;
  std::vector<std::string> positional;
};

class CommandRouter {
public:
  [[nodiscard]] Command parse(int argc, char** argv) const;
  [[nodiscard]] static std::string help_text();
  [[nodiscard]] static std::string version_text();
};

}  // namespace localagent
