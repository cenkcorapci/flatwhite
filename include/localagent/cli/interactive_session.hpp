#pragma once

#include "localagent/app/session_controller.hpp"
#include "localagent/cli/resource_monitor.hpp"
#include "localagent/config/config.hpp"
#include "localagent/model/backend.hpp"

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace localagent {

enum class InteractiveCommandKind {
  Prompt,
  Help,
  Exit,
  NewSession,
  History,
  Sessions,
  Resume,
  Clear,
  Model,
  Invalid,
};

struct InteractiveCommand {
  InteractiveCommandKind kind{InteractiveCommandKind::Prompt};
  std::string argument;
};

enum class ModelSelectionStatus { Selected, NoMatch, Ambiguous };

struct ModelSelection {
  ModelSelectionStatus status{ModelSelectionStatus::NoMatch};
  std::string model;
  std::vector<std::string> matches;
};

[[nodiscard]] InteractiveCommand parse_interactive_command(const std::string& line);
[[nodiscard]] ModelSelection select_model(const std::vector<ModelInfo>& models,
                                          const std::string& query);

class InteractiveSession {
public:
  InteractiveSession(const Config& config, SessionController& sessions,
                     std::shared_ptr<ModelBackend> model, std::istream& input,
                     std::ostream& output, std::ostream& error, bool use_color);

  [[nodiscard]] int run();

private:
  void write_status(const std::string& label, const std::string& message);
  void write_failure(const std::string& message);
  void write_resource_usage();
  void print_sessions();
  void handle_model(const std::string& query);
  [[nodiscard]] int run_prompt(const std::string& prompt);

  const Config& config_;
  SessionController& sessions_;
  std::shared_ptr<ModelBackend> model_;
  std::istream& input_;
  std::ostream& output_;
  std::ostream& error_;
  bool use_color_;
  SessionId session_id_;
  std::string active_model_;
  std::vector<std::string> history_;
  ResourceMonitor resource_monitor_;
};

}  // namespace localagent
