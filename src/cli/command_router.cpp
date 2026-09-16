#include "localagent/cli/command_router.hpp"

namespace localagent {

namespace {

std::optional<CommandKind> kind_from_mode(AgentMode mode) {
  switch (mode) {
    case AgentMode::Agent:
      return CommandKind::Agent;
    case AgentMode::Plan:
      return CommandKind::Plan;
    case AgentMode::Ask:
      return CommandKind::Ask;
    case AgentMode::Debug:
      return CommandKind::Debug;
    case AgentMode::Review:
      return CommandKind::Review;
    case AgentMode::Chat:
      return CommandKind::Chat;
    case AgentMode::Data:
      return CommandKind::Data;
    case AgentMode::Research:
      return CommandKind::Research;
  }
  return std::nullopt;
}

}  // namespace

Command CommandRouter::parse(int argc, char** argv) const {
  Command cmd;
  if (argc <= 1) {
    cmd.kind = CommandKind::Help;
    return cmd;
  }

  int i = 1;
  const std::string first = argv[i];
  if (first == "--help" || first == "-h") {
    cmd.kind = CommandKind::Help;
    return cmd;
  }
  if (first == "--version" || first == "-v") {
    cmd.kind = CommandKind::Version;
    return cmd;
  }

  auto set_mode = [&](AgentMode mode) {
    cmd.mode = mode;
    if (auto kind = kind_from_mode(mode)) {
      cmd.kind = *kind;
    }
  };

  if (first == "chat") {
    set_mode(AgentMode::Chat);
    ++i;
  } else if (first == "agent") {
    set_mode(AgentMode::Agent);
    ++i;
  } else if (first == "plan") {
    set_mode(AgentMode::Plan);
    ++i;
  } else if (first == "ask") {
    set_mode(AgentMode::Ask);
    ++i;
  } else if (first == "debug") {
    set_mode(AgentMode::Debug);
    ++i;
  } else if (first == "review") {
    set_mode(AgentMode::Review);
    ++i;
  } else if (first == "data") {
    set_mode(AgentMode::Data);
    ++i;
  } else if (first == "research") {
    set_mode(AgentMode::Research);
    ++i;
  } else if (first == "sessions") {
    ++i;
    if (i < argc && std::string_view(argv[i]) == "list") {
      cmd.kind = CommandKind::SessionsList;
      ++i;
    } else if (i < argc && std::string_view(argv[i]) == "resume") {
      cmd.kind = CommandKind::SessionsResume;
      ++i;
      if (i < argc) {
        cmd.session_id = argv[i++];
      }
    }
  } else if (first == "models") {
    ++i;
    if (i < argc && std::string_view(argv[i]) == "list") {
      cmd.kind = CommandKind::ModelsList;
      ++i;
    }
  } else if (first == "localagent") {
    ++i;
  } else {
    cmd.kind = CommandKind::Chat;
  }

  for (; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-p" || arg == "--prompt") {
      if (i + 1 < argc) {
        cmd.prompt = argv[++i];
        cmd.kind = CommandKind::Prompt;
      }
    } else if (arg == "--output-format" && i + 1 < argc) {
      const std::string fmt = argv[++i];
      if (fmt == "json") {
        cmd.output_format = OutputFormat::Json;
      } else {
        cmd.output_format = OutputFormat::Text;
      }
    } else if (arg == "--continue") {
      cmd.continue_session = true;
    } else if (arg == "--help" || arg == "-h") {
      cmd.kind = CommandKind::Help;
      return cmd;
    } else if (arg == "--version" || arg == "-v") {
      cmd.kind = CommandKind::Version;
      return cmd;
    } else if (cmd.prompt.empty() && cmd.kind != CommandKind::SessionsList &&
               cmd.kind != CommandKind::SessionsResume &&
               cmd.kind != CommandKind::ModelsList) {
      cmd.prompt = arg;
    } else {
      cmd.positional.push_back(arg);
    }
  }

  if (cmd.kind == CommandKind::Help && !cmd.prompt.empty()) {
    cmd.kind = CommandKind::Prompt;
  }

  return cmd;
}

std::string CommandRouter::help_text() {
  return R"(localagent - local coding agent runtime

Usage:
  localagent [chat|agent|plan|ask|debug|review|data|research] [options] [prompt]
  localagent -p "prompt" --output-format text|json
  localagent sessions list|resume <id>
  localagent models list

Options:
  -p, --prompt           Non-interactive prompt
  --output-format        text or json
  --continue             Resume previous session
  -h, --help             Show help
  -v, --version          Show version
)";
}

std::string CommandRouter::version_text() { return "localagent 0.1.0"; }

}  // namespace localagent
