#include "localagent/cli/interactive_session.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

TEST_CASE("Interactive commands parse supported slash commands", "[interactive]") {
  const auto help = parse_interactive_command("/help");
  CHECK(help.kind == InteractiveCommandKind::Help);
  const auto resume = parse_interactive_command("/resume saved-session");
  CHECK(resume.kind == InteractiveCommandKind::Resume);
  CHECK(resume.argument == "saved-session");
  const auto prompt = parse_interactive_command("explain this repository");
  CHECK(prompt.kind == InteractiveCommandKind::Prompt);
  CHECK(prompt.argument == "explain this repository");
  CHECK(parse_interactive_command("/quit").kind == InteractiveCommandKind::Exit);
  CHECK(parse_interactive_command("/unknown").kind == InteractiveCommandKind::Invalid);
}

TEST_CASE("Interactive model selection accepts exact and unique prefixes", "[interactive]") {
  const std::vector<ModelInfo> models = {
      {.id = ModelId{"llama3.2"}, .display_name = "llama3.2"},
      {.id = ModelId{"qwen2.5-coder"}, .display_name = "qwen2.5-coder"},
      {.id = ModelId{"qwen3"}, .display_name = "qwen3"},
  };
  const auto exact = select_model(models, "LLAMA3.2");
  CHECK(exact.status == ModelSelectionStatus::Selected);
  CHECK(exact.model == "llama3.2");
  const auto unique = select_model(models, "qwen2.5");
  CHECK(unique.status == ModelSelectionStatus::Selected);
  CHECK(unique.model == "qwen2.5-coder");
  CHECK(select_model(models, "qwen").status == ModelSelectionStatus::Ambiguous);
  CHECK(select_model(models, "missing").status == ModelSelectionStatus::NoMatch);
}
