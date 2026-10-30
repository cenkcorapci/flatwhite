#include "localagent/cli/interactive_session.hpp"

#include "localagent/agent/runtime/agent_runtime.hpp"
#include "localagent/persistence/event_log.hpp"
#include "localagent/permissions/permission_engine.hpp"
#include "localagent/tools/tool_registry.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <iostream>
#include <sstream>

namespace localagent {
namespace {

std::string trim(std::string value) {
  const auto begin = value.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return {};
  }
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(begin, end - begin + 1);
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::string interactive_help() {
  return R"(Commands:
  /help                 Show this help
  /exit, /quit          Leave Flatwhite
  /new                  Start a new session
  /history              Show prompts from this session
  /sessions             List saved sessions
  /resume [id]          Resume a session, or the most recent session
  /clear                Clear the terminal
  /model [name-prefix] List local Ollama models or select one
)";
}

}  // namespace

InteractiveCommand parse_interactive_command(const std::string& line) {
  const auto text = trim(line);
  if (text.empty() || text.front() != '/') {
    return InteractiveCommand{.kind = InteractiveCommandKind::Prompt, .argument = text};
  }

  const auto separator = text.find_first_of(" \t");
  const auto command = lower(text.substr(0, separator));
  const auto argument =
      separator == std::string::npos ? std::string{} : trim(text.substr(separator + 1));
  if (command == "/help") return {.kind = InteractiveCommandKind::Help, .argument = argument};
  if (command == "/exit" || command == "/quit") {
    return {.kind = InteractiveCommandKind::Exit, .argument = argument};
  }
  if (command == "/new") return {.kind = InteractiveCommandKind::NewSession, .argument = argument};
  if (command == "/history") {
    return {.kind = InteractiveCommandKind::History, .argument = argument};
  }
  if (command == "/sessions") {
    return {.kind = InteractiveCommandKind::Sessions, .argument = argument};
  }
  if (command == "/resume") {
    return {.kind = InteractiveCommandKind::Resume, .argument = argument};
  }
  if (command == "/clear") return {.kind = InteractiveCommandKind::Clear, .argument = argument};
  if (command == "/model") return {.kind = InteractiveCommandKind::Model, .argument = argument};
  return {.kind = InteractiveCommandKind::Invalid, .argument = command};
}

ModelSelection select_model(const std::vector<ModelInfo>& models, const std::string& query) {
  const auto needle = lower(trim(query));
  if (needle.empty()) {
    return {};
  }

  ModelSelection selection;
  for (const auto& model : models) {
    const auto name = model.id.str();
    if (lower(name) == needle) {
      return {.status = ModelSelectionStatus::Selected, .model = name};
    }
    if (lower(name).starts_with(needle)) {
      selection.matches.push_back(name);
    }
  }
  if (selection.matches.size() == 1) {
    selection.status = ModelSelectionStatus::Selected;
    selection.model = selection.matches.front();
  } else if (selection.matches.size() > 1) {
    selection.status = ModelSelectionStatus::Ambiguous;
  }
  return selection;
}

InteractiveSession::InteractiveSession(const Config& config, SessionController& sessions,
                                       std::shared_ptr<ModelBackend> model, std::istream& input,
                                       std::ostream& output, std::ostream& error, bool use_color)
    : config_(config),
      sessions_(sessions),
      model_(std::move(model)),
      input_(input),
      output_(output),
      error_(error),
      use_color_(use_color),
      session_id_(sessions_.create("interactive session")),
      active_model_(config.default_model) {}

void InteractiveSession::write_status(const std::string& label, const std::string& message) {
  if (use_color_) {
    output_ << "\033[36m" << label << "\033[0m " << message << "\n";
  } else {
    output_ << label << " " << message << "\n";
  }
}

void InteractiveSession::write_failure(const std::string& message) {
  if (use_color_) {
    error_ << "\033[31mfailure\033[0m " << message << "\n";
  } else {
    error_ << "failure " << message << "\n";
  }
}

void InteractiveSession::write_resource_usage() {
  if (!use_color_) {
    return;
  }
  output_ << "\033[2m" << format_resource_usage(resource_monitor_.sample()) << "\033[0m\n";
}

void InteractiveSession::print_sessions() {
  const auto records = sessions_.list();
  if (records.empty()) {
    output_ << "No saved sessions.\n";
    return;
  }
  for (const auto& session : records) {
    output_ << session.id.str() << "\t" << session.title << "\t" << session.updated_at << "\n";
  }
}

void InteractiveSession::handle_model(const std::string& query) {
  try {
    const auto models = model_->list_models();
    if (query.empty()) {
      if (models.empty()) {
        output_ << "No local Ollama models found.\n";
        return;
      }
      for (const auto& item : models) {
        output_ << item.id.str();
        if (item.id.str() == active_model_) {
          output_ << " (selected)";
        }
        output_ << "\n";
      }
      return;
    }

    const auto selection = select_model(models, query);
    if (selection.status == ModelSelectionStatus::Selected) {
      active_model_ = selection.model;
      write_status("model", "selected " + active_model_);
    } else if (selection.status == ModelSelectionStatus::Ambiguous) {
      std::string message = "model selection is ambiguous: " + query + " (matches:";
      for (const auto& match : selection.matches) message += " " + match;
      write_failure(message + ")");
    } else {
      write_failure("model not found: " + query);
    }
  } catch (const std::exception& exception) {
    write_failure("unable to list local Ollama models: " + std::string{exception.what()});
  }
}

int InteractiveSession::run_prompt(const std::string& prompt) {
  history_.push_back(prompt);
  write_status("request", "running with " + active_model_);
  try {
    PermissionEngine permissions(config_.workspace_root, config_.permissions);
    AgentRuntime runtime(model_, ToolRegistry{}, permissions);
    runtime.set_event_log(std::make_shared<EventLog>(config_.data_dir / "events.jsonl"));
    RunRequest request;
    request.session_id = session_id_;
    request.mode = AgentMode::Chat;
    request.goal = prompt;
    request.workspace_root = config_.workspace_root;
    request.model = ModelId{active_model_};
    request.web_search_endpoint = config_.web_search.endpoint;
    const auto result = runtime.run(request);
    if (result.completion == RunCompletion::Failed) {
      write_failure("request failed: " + result.final_answer);
      return 1;
    }
    write_status("success", "answer");
    if (use_color_) output_ << "\033[37m";
    output_ << result.final_answer;
    if (use_color_) output_ << "\033[0m";
    output_ << "\n";
    return 0;
  } catch (const std::exception& exception) {
    write_failure("request failed: " + std::string{exception.what()});
    return 1;
  }
}

int InteractiveSession::run() {
  write_status("flatwhite", "interactive session — /help for commands");
  std::string line;
  while (true) {
    if (use_color_) {
      write_resource_usage();
      output_ << "\033[32mflatwhite>\033[0m " << std::flush;
    }
    if (!std::getline(input_, line)) {
      output_ << "\n";
      return 0;
    }
    const auto command = parse_interactive_command(line);
    switch (command.kind) {
      case InteractiveCommandKind::Prompt:
        if (!command.argument.empty()) static_cast<void>(run_prompt(command.argument));
        break;
      case InteractiveCommandKind::Help:
        output_ << interactive_help();
        break;
      case InteractiveCommandKind::Exit:
        return 0;
      case InteractiveCommandKind::NewSession:
        session_id_ = sessions_.create("interactive session");
        history_.clear();
        write_status("session", "started " + session_id_.str());
        break;
      case InteractiveCommandKind::History:
        for (std::size_t i = 0; i < history_.size(); ++i) {
          output_ << (i + 1) << "  " << history_[i] << "\n";
        }
        break;
      case InteractiveCommandKind::Sessions:
        print_sessions();
        break;
      case InteractiveCommandKind::Resume:
        if (const auto resumed = sessions_.resume(
                command.argument.empty() ? std::nullopt
                                         : std::optional<std::string>{command.argument},
                true)) {
          session_id_ = *resumed;
          write_status("session", "resumed " + session_id_.str());
        } else {
          write_failure("session not found");
        }
        break;
      case InteractiveCommandKind::Clear:
        if (use_color_) output_ << "\033[2J\033[H";
        break;
      case InteractiveCommandKind::Model:
        handle_model(command.argument);
        break;
      case InteractiveCommandKind::Invalid:
        write_failure("unknown command: " + command.argument + " (try /help)");
        break;
    }
  }
}

}  // namespace localagent
