#pragma once

#include "localagent/agent/budget/budget.hpp"
#include "localagent/agent/state_machine/state_machine.hpp"
#include "localagent/common/cancellation.hpp"
#include "localagent/common/strong_id.hpp"
#include "localagent/context/context_builder.hpp"
#include "localagent/model/backend.hpp"
#include "localagent/model/model_types.hpp"
#include "localagent/persistence/event_log.hpp"
#include "localagent/permissions/permission_engine.hpp"
#include "localagent/tools/tool_registry.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace localagent {

enum class RunCompletion { ImplementedVerified, CompletedUnverified, Failed, Cancelled };

struct AgentRun {
  RunId id;
  SessionId session_id;
  AgentMode mode{AgentMode::Agent};
  AgentState state{AgentState::Initializing};
  RunCompletion completion{RunCompletion::Failed};
  std::string final_answer;
  Budget budget;
  uint32_t turns{0};
};

struct RunRequest {
  SessionId session_id;
  AgentMode mode{AgentMode::Agent};
  std::string goal;
  std::filesystem::path workspace_root;
  bool verify{true};
  Budget budget{};
  CancellationToken cancellation;
  ModelId model{ModelId{"fast-code"}};
};

class AgentRuntime {
public:
  AgentRuntime(std::shared_ptr<ModelBackend> model, ToolRegistry tools,
                PermissionEngine permissions);

  void set_event_log(std::shared_ptr<EventLog> log) { event_log_ = std::move(log); }

  [[nodiscard]] AgentRun run(const RunRequest& request);

private:
  std::shared_ptr<ModelBackend> model_;
  ToolRegistry tools_;
  PermissionEngine permissions_;
  std::shared_ptr<EventLog> event_log_;

  void log_event(const RunId& run_id, std::string type, const nlohmann::json& payload) const;
  [[nodiscard]] ChatRequest build_chat_request(const RunRequest& request,
                                               const BuiltContext& ctx,
                                               const std::vector<ToolDescriptor>& tools) const;
};

}  // namespace localagent
