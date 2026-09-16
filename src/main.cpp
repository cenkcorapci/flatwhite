#include "localagent/agent/runtime/agent_runtime.hpp"
#include "localagent/app/session_controller.hpp"
#include "localagent/cli/command_router.hpp"
#include "localagent/config/config.hpp"
#include "localagent/model/ollama/ollama_backend.hpp"
#include "localagent/persistence/event_log.hpp"
#include "localagent/permissions/permission_engine.hpp"
#include "localagent/tools/tool_registry.hpp"

#include <iostream>
#include <memory>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace {

std::shared_ptr<localagent::ModelBackend> make_model_backend(const localagent::Config& cfg) {
  using localagent::ollama::FakeModelBackend;
  using localagent::ollama::OllamaBackend;
  using localagent::ollama::OllamaConfig;

  if (cfg.use_fake_model) {
    spdlog::warn("using FakeModelBackend");
    auto backend = std::make_shared<FakeModelBackend>();
    backend->enqueue(FakeModelBackend::ScriptedResponse{
        .response =
            localagent::ChatResponse{
                .message = localagent::Message{.role = localagent::MessageRole::Assistant},
                .tool_calls =
                    {localagent::ToolCall{
                        .id = "call-1",
                        .name = "read_file",
                        .arguments_json = R"({"path":"README.md"})",
                    }},
            },
    });
    backend->enqueue(FakeModelBackend::ScriptedResponse{
        .response =
            localagent::ChatResponse{
                .message =
                    localagent::Message{
                        .role = localagent::MessageRole::Assistant,
                        .content = "Hello from localagent.",
                    },
            },
    });
    return backend;
  }

  OllamaConfig config;
  config.base_url = cfg.ollama_host;
  if (localagent::ollama::ollama_reachable(config.base_url)) {
    return std::make_shared<OllamaBackend>(config);
  }

  spdlog::warn("ollama unreachable; falling back to FakeModelBackend");
  return std::make_shared<FakeModelBackend>();
}

}  // namespace

int main(int argc, char** argv) {
  spdlog::set_level(spdlog::level::info);
  const auto cfg = localagent::load_config(argc, argv);
  localagent::CommandRouter router;
  const auto cmd = router.parse(argc, argv);

  if (cmd.kind == localagent::CommandKind::Help) {
    std::cout << localagent::CommandRouter::help_text();
    return 0;
  }
  if (cmd.kind == localagent::CommandKind::Version) {
    std::cout << localagent::CommandRouter::version_text() << "\n";
    return 0;
  }

  localagent::SessionController sessions(cfg.data_dir);
  if (cmd.kind == localagent::CommandKind::SessionsList) {
    for (const auto& s : sessions.list()) {
      std::cout << s.id.str() << "\t" << s.title << "\t" << s.updated_at << "\n";
    }
    return 0;
  }

  localagent::SessionId session_id =
      sessions.create(cmd.prompt.empty() ? "session" : cmd.prompt.substr(0, 64));
  if (cmd.continue_session || cmd.kind == localagent::CommandKind::SessionsResume) {
    if (auto resumed = sessions.resume(cmd.session_id, true)) {
      session_id = *resumed;
    }
  }

  if (cmd.kind == localagent::CommandKind::ModelsList) {
    std::cout << cfg.default_model << "\n";
    return 0;
  }

  if (cmd.prompt.empty()) {
    std::cerr << "prompt required; use -p \"...\" or pass as argument\n";
    return 2;
  }

  auto model = make_model_backend(cfg);
  localagent::PermissionEngine permissions(cfg.workspace_root, cfg.permissions);
  localagent::ToolRegistry tools;
  localagent::AgentRuntime runtime(model, std::move(tools), permissions);
  runtime.set_event_log(
      std::make_shared<localagent::EventLog>(cfg.data_dir / "events.jsonl"));

  localagent::RunRequest req;
  req.session_id = session_id;
  req.mode = cmd.mode;
  req.goal = cmd.prompt;
  req.workspace_root = cfg.workspace_root;
  req.verify = cmd.mode == localagent::AgentMode::Agent;
  req.model = localagent::ModelId{cfg.default_model};

  const auto run = runtime.run(req);

  if (cmd.output_format == localagent::OutputFormat::Json) {
    nlohmann::json out{{"answer", run.final_answer},
                       {"state", localagent::to_string(run.state)},
                       {"completion", static_cast<int>(run.completion)}};
    std::cout << out.dump(2) << "\n";
  } else {
    std::cout << run.final_answer << "\n";
  }

  return run.completion == localagent::RunCompletion::Failed ? 1 : 0;
}
