#include "localagent/agent/runtime/agent_runtime.hpp"
#include "localagent/model/ollama/ollama_backend.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

using namespace localagent;

namespace {

std::filesystem::path workspace() {
  const auto root = std::filesystem::temp_directory_path() / "localagent_runtime_test";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  std::ofstream(root / "README.md") << "runtime test\n";
  return root;
}

std::shared_ptr<ollama::FakeModelBackend> mock_model() {
  auto backend = std::make_shared<ollama::FakeModelBackend>();
  backend->enqueue(ollama::FakeModelBackend::ScriptedResponse{
      .response =
          ChatResponse{
              .message = Message{.role = MessageRole::Assistant},
              .tool_calls =
                  {ToolCall{.id = "c1", .name = "read_file", .arguments_json = R"({"path":"README.md"})"}},
              .usage = TokenUsage{.prompt_tokens = 5, .completion_tokens = 5, .total_tokens = 10},
          },
  });
  backend->enqueue(ollama::FakeModelBackend::ScriptedResponse{
      .response =
          ChatResponse{
              .message =
                  Message{.role = MessageRole::Assistant, .content = "Task complete."},
              .usage = TokenUsage{.prompt_tokens = 5, .completion_tokens = 5, .total_tokens = 10},
          },
  });
  return backend;
}

}  // namespace

TEST_CASE("AgentRuntime executes tool calls then completes", "[agent_runtime]") {
  const auto root = workspace();
  PermissionEngine permissions(true);
  ToolRegistry tools;
  AgentRuntime runtime(mock_model(), std::move(tools), permissions);

  RunRequest req;
  req.session_id = SessionId{"sess-1"};
  req.mode = AgentMode::Ask;
  req.goal = "read readme";
  req.workspace_root = root;
  req.verify = false;
  req.budget.max_turns = 5;

  const auto run = runtime.run(req);
  CHECK(run.state == AgentState::Completed);
  CHECK(run.final_answer == "Task complete.");
}

TEST_CASE("AgentRuntime runs a development workflow without approval", "[agent_runtime]") {
  const auto root = workspace();
  auto backend = std::make_shared<ollama::FakeModelBackend>();
  backend->enqueue({.response = ChatResponse{
                       .tool_calls = {ToolCall{.id = "write", .name = "write_file",
                                               .arguments_json =
                                                   R"({"path":"generated.txt","content":"ready"})"}}}});
  backend->enqueue({.response = ChatResponse{
                       .tool_calls = {ToolCall{.id = "shell", .name = "shell",
                                               .arguments_json =
                                                   R"({"command":"test -f generated.txt"})"}}}});
  backend->enqueue({.response = ChatResponse{
                       .tool_calls = {ToolCall{.id = "git", .name = "git_status",
                                               .arguments_json = "{}"}}}});
  backend->enqueue({.response = ChatResponse{
                       .message = Message{.role = MessageRole::Assistant,
                                          .content = "Development complete."}}});

  PermissionsConfig config;
  PermissionEngine permissions{root, config};
  AgentRuntime runtime(backend, ToolRegistry{}, permissions);
  RunRequest req;
  req.session_id = SessionId{"sess-2"};
  req.mode = AgentMode::Ask;
  req.goal = "create and check a file";
  req.workspace_root = root;
  req.verify = false;
  req.budget.max_turns = 8;

  const auto run = runtime.run(req);
  REQUIRE(run.state == AgentState::Completed);
  REQUIRE(run.final_answer == "Development complete.");
  std::ifstream generated(root / "generated.txt");
  std::string content;
  std::getline(generated, content);
  CHECK(content == "ready");
}
