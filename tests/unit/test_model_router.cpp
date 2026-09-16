#include <localagent/model/ollama/ollama_backend.hpp>
#include <localagent/model/router/model_router.hpp>

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

namespace {

RoutingRequest base_request() {
  RoutingRequest request;
  request.required_capabilities = capability(Capability::Chat) | Capability::Code;
  request.complexity = 0.3f;
  request.preference = RoutingPreference::PreferCheap;
  return request;
}

}  // namespace

TEST_CASE("ModelRegistry loads default fast-code and main-reasoner models", "[model_router]") {
  ModelRegistry registry;
  REQUIRE(registry.models().size() >= 2);

  const auto fast = registry.find(ModelId{"fast-code"});
  const auto reasoner = registry.find(ModelId{"main-reasoner"});
  REQUIRE(fast.has_value());
  REQUIRE(reasoner.has_value());
  CHECK(fast->cost_score < reasoner->cost_score);
  CHECK(reasoner->quality_score > fast->quality_score);
}

TEST_CASE("ModelRouter prefers cheaper model when quality sufficient", "[model_router]") {
  ModelRouter router;
  auto request = base_request();
  request.complexity = 0.25f;
  request.preference = RoutingPreference::PreferCheap;

  const auto decision = router.route(request);
  CHECK(decision.model == ModelId{"fast-code"});
  CHECK(decision.score > 0.0f);
  CHECK_FALSE(decision.escalated);
}

TEST_CASE("ModelRouter selects reasoner for high complexity", "[model_router]") {
  ModelRouter router;
  auto request = base_request();
  request.complexity = 0.9f;
  request.required_capabilities =
      capability(Capability::Chat) | Capability::Code | Capability::Reasoning;
  request.preference = RoutingPreference::PreferQuality;

  const auto decision = router.route(request);
  CHECK(decision.model == ModelId{"main-reasoner"});
}

TEST_CASE("ModelRouter excludes models missing required capabilities", "[model_router]") {
  ModelRegistry registry;
  registry.add(ModelInfo{
      .id = ModelId{"embed-only"},
      .display_name = "embed-only",
      .capabilities = capability(Capability::Embedding),
      .quality_score = 0.99f,
      .speed_score = 1.0f,
      .memory_mb = 512,
      .cost_score = 0.05f,
  });

  ModelRouter router(std::move(registry));
  auto request = base_request();
  request.required_capabilities = capability(Capability::Chat) | Capability::ToolUse;

  const auto decision = router.route(request);
  CHECK(decision.model != ModelId{"embed-only"});
}

TEST_CASE("ModelRouter escalates on low observed quality", "[model_router]") {
  ModelRouter router;
  RoutingRequest request;
  request.required_capabilities = capability(Capability::Chat) | Capability::Code;
  request.complexity = 0.8f;
  request.observed_quality = 0.4f;

  const auto escalation =
      router.check_escalation(request, ModelId{"fast-code"}, /*retry_count=*/0);
  REQUIRE(escalation.has_value());
  CHECK(escalation->escalated);
  CHECK(escalation->model == ModelId{"main-reasoner"});
}

TEST_CASE("ModelRouter does not escalate when quality is acceptable", "[model_router]") {
  ModelRouter router;
  RoutingRequest request;
  request.complexity = 0.4f;
  request.observed_quality = 0.9f;

  const auto escalation =
      router.check_escalation(request, ModelId{"fast-code"}, /*retry_count=*/0);
  CHECK_FALSE(escalation.has_value());
}

TEST_CASE("ModelRouter classify delegates to classify_task", "[model_router]") {
  ModelRouter router;
  CHECK(router.classify("debug crash in parser", AgentMode::Debug) == TaskCategory::Debugging);
  CHECK(router.classify("design system architecture", AgentMode::Agent) ==
        TaskCategory::Architecture);
}

TEST_CASE("FakeModelBackend returns deterministic responses without network", "[model_router]") {
  ollama::FakeModelBackend backend;
  ChatRequest request;
  request.model = ModelId{"fake-model"};
  request.messages.push_back(Message{.role = MessageRole::User, .content = "hi"});

  const auto response = backend.chat(request);
  CHECK(response.message.content == "fake response");
  CHECK(response.model == "fake-model");
  CHECK(backend.chat_call_count() == 1);
}

TEST_CASE("FakeModelBackend supports scripted stream chunks", "[model_router]") {
  ollama::FakeModelBackend backend;
  ollama::FakeModelBackend::ScriptedResponse scripted;
  scripted.stream_chunks = {
      ChatChunk{.content_delta = "a"},
      ChatChunk{.content_delta = "b", .done = true},
  };
  scripted.response.message = Message{.role = MessageRole::Assistant, .content = "ab"};
  backend.enqueue(std::move(scripted));

  ChatRequest request;
  request.model = ModelId{"fake-model"};
  std::string streamed;
  const auto response = backend.chat(request, [&](const ChatChunk& chunk) {
    streamed += chunk.content_delta;
  });

  CHECK(streamed == "ab");
  CHECK(response.message.content == "ab");
}
