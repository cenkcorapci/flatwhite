#include "localagent/agent/runtime/agent_runtime.hpp"

#include "localagent/agent/budget/budget.hpp"
#include "localagent/agent/verifier/verifier.hpp"
#include "localagent/common/strong_id.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>

namespace localagent {

AgentRuntime::AgentRuntime(std::shared_ptr<ModelBackend> model, ToolRegistry tools,
                           PermissionEngine permissions)
    : model_(std::move(model)), tools_(std::move(tools)), permissions_(std::move(permissions)) {}

void AgentRuntime::log_event(const RunId& run_id, std::string type,
                             const nlohmann::json& payload) const {
  if (!event_log_) {
    return;
  }
  EventType event_type = EventType::StateChanged;
  if (type == "run_started") {
    event_type = EventType::SessionStarted;
  } else if (type == "model_response") {
    event_type = EventType::AssistantMessage;
  } else if (type == "tool_result") {
    event_type = EventType::ToolCompleted;
  } else if (type == "verification") {
    event_type = payload.value("build_ok", false) && payload.value("tests_ok", false)
                     ? EventType::VerificationPassed
                     : EventType::VerificationFailed;
  } else if (type == "run_finished") {
    event_type = EventType::RunCompleted;
  }
  event_log_->append(run_id, event_type, payload);
}

ChatRequest AgentRuntime::build_chat_request(const RunRequest& request, const BuiltContext& ctx,
                                             const std::vector<ToolDescriptor>& tools) const {
  ChatRequest chat;
  chat.model = request.model;

  std::ostringstream system;
  system << "You are localagent. Workspace: " << request.workspace_root.string() << "\n\n";
  for (const auto& item : ctx.items) {
    system << "[" << item.label << "]\n" << item.content << "\n\n";
  }
  chat.messages.push_back(Message{.role = MessageRole::System, .content = system.str()});
  chat.messages.push_back(Message{.role = MessageRole::User, .content = request.goal});

  for (const auto& tool : tools) {
    chat.tools.push_back(ToolDefinition{
        .name = tool.name,
        .description = tool.description,
        .parameters_json = tool.parameters_schema.dump(),
    });
  }

  return chat;
}

AgentRun AgentRuntime::run(const RunRequest& request) {
  AgentRun run;
  run.id = RunId{make_uuid()};
  run.session_id = request.session_id;
  run.mode = request.mode;
  run.budget = request.budget.max_turns ? request.budget : default_budget_for(request.mode);

  StateMachine sm(AgentState::Initializing);
  run.state = sm.state();

  ToolContext tool_ctx{request.workspace_root, &permissions_, request.cancellation};
  tools_.register_defaults(tool_ctx);

  log_event(run.id, "run_started",
            {{"goal", request.goal}, {"mode", to_string(request.mode)}});

  ContextBuilder builder(request.workspace_root);
  ContextBuildRequest ctx_req;
  ctx_req.goal = request.goal;
  ctx_req.token_budget = run.budget.max_input_tokens;

  bool verified = false;
  bool had_tool_calls = false;

  while (!is_terminal_state(sm.state())) {
    request.cancellation.throw_if_cancelled();
    if (budget_exhausted(run.budget)) {
      static_cast<void>(sm.transition(AgentState::Failed));
      run.final_answer = "Budget exhausted";
      break;
    }

    switch (sm.state()) {
      case AgentState::Initializing:
        static_cast<void>(sm.transition(AgentState::BuildingContext));
        break;

      case AgentState::BuildingContext: {
        const auto built = builder.build(ctx_req);
        ctx_req.conversation.push_back({"assistant", "context built"});
        static_cast<void>(sm.transition(AgentState::AwaitingModel));
        run.state = sm.state();
        log_event(run.id, "context_built", {{"items", built.items.size()}});
        break;
      }

      case AgentState::AwaitingModel: {
        const auto built = builder.build(ctx_req);
        const auto tool_desc = tools_.list_descriptors();
        const auto chat_req = build_chat_request(request, built, tool_desc);
        request.cancellation.throw_if_cancelled();
        const auto response = model_->chat(chat_req);

        run.budget.used_turns += 1;
        if (response.usage) {
          run.budget.used_input_tokens += response.usage->prompt_tokens;
          run.budget.used_output_tokens += response.usage->completion_tokens;
        }
        run.turns += 1;

        static_cast<void>(sm.transition(AgentState::ParsingResponse));
        run.state = sm.state();
        log_event(run.id, "model_response",
                  {{"content", response.message.content},
                   {"tool_calls", response.tool_calls.size()}});

        if (!response.tool_calls.empty()) {
          static_cast<void>(sm.transition(AgentState::ExecutingTools));
          for (const auto& call : response.tool_calls) {
            request.cancellation.throw_if_cancelled();
            nlohmann::json args = nlohmann::json::object();
            if (!call.arguments_json.empty()) {
              args = nlohmann::json::parse(call.arguments_json, nullptr, false);
              if (args.is_discarded()) {
                args = nlohmann::json::object();
              }
            }
            ToolRequest treq{call.name, args, call.id};
            const auto result = tools_.dispatch(treq, tool_ctx);
            had_tool_calls = true;
            run.budget.used_tool_calls += 1;

            if (result && result->error &&
                result->error->category == ErrorCategory::Permission) {
              static_cast<void>(sm.transition(AgentState::Failed));
              run.final_answer = result->error->message;
              break;
            }

            std::string observation = result ? result->content : "tool failed";
            ctx_req.conversation.push_back({"tool", observation});
            log_event(run.id, "tool_result",
                      {{"tool", call.name}, {"success", result && result->success}});
          }
          if (sm.state() != AgentState::Failed) {
            static_cast<void>(sm.transition(AgentState::Observing));
          }
        } else {
          run.final_answer = response.message.content;
          if (request.verify && request.mode == AgentMode::Agent) {
            static_cast<void>(sm.transition(AgentState::Verifying));
          } else {
            static_cast<void>(sm.transition(AgentState::Completed));
            run.completion = RunCompletion::CompletedUnverified;
          }
        }
        run.state = sm.state();
        break;
      }

      case AgentState::Verifying: {
        Verifier verifier(request.workspace_root);
        const auto build = verifier.verify_build();
        const auto tests = verifier.verify_tests();
        verified = build.success && tests.success;
        log_event(run.id, "verification",
                  {{"build_ok", build.success}, {"tests_ok", tests.success}});
        if (verified) {
          static_cast<void>(sm.transition(AgentState::Completed));
          run.completion = RunCompletion::ImplementedVerified;
        } else if (had_tool_calls) {
          static_cast<void>(sm.transition(AgentState::Completed));
          run.completion = RunCompletion::CompletedUnverified;
          run.final_answer += "\n[verification failed]";
        } else {
          static_cast<void>(sm.transition(AgentState::Completed));
          run.completion = RunCompletion::CompletedUnverified;
        }
        run.state = sm.state();
        break;
      }

      case AgentState::Observing:
        static_cast<void>(sm.transition(AgentState::AwaitingModel));
        run.state = sm.state();
        break;

      default:
        static_cast<void>(sm.transition(AgentState::Failed));
        run.final_answer = "unexpected state";
        break;
    }
  }

  run.state = sm.state();
  if (sm.state() == AgentState::Completed && run.completion == RunCompletion::Failed) {
    run.completion = RunCompletion::CompletedUnverified;
  }
  if (sm.state() == AgentState::Failed) {
    run.completion = RunCompletion::Failed;
  }

  log_event(run.id, "run_finished",
            {{"completion", static_cast<int>(run.completion)}, {"answer", run.final_answer}});
  spdlog::info("run {} finished in state {}", run.id.str(), to_string(run.state));
  return run;
}

}  // namespace localagent
