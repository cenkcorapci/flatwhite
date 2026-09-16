#include <localagent/model/router/model_router.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace localagent {
namespace {

ModelInfo make_fast_code_model() {
  return ModelInfo{
      .id = ModelId{"fast-code"},
      .display_name = "fast-code",
      .capabilities = capability(Capability::Chat) | Capability::ToolUse | Capability::Code,
      .context_length = 32768,
      .quality_score = 0.72f,
      .speed_score = 1.0f,
      .memory_mb = 4096,
      .cost_score = 0.2f,
  };
}

ModelInfo make_main_reasoner_model() {
  return ModelInfo{
      .id = ModelId{"main-reasoner"},
      .display_name = "main-reasoner",
      .capabilities = capability(Capability::Chat) | Capability::ToolUse | Capability::Code |
                      Capability::Reasoning,
      .context_length = 128000,
      .quality_score = 0.95f,
      .speed_score = 0.55f,
      .memory_mb = 16384,
      .cost_score = 0.85f,
  };
}

ModelInfo make_embed_model() {
  return ModelInfo{
      .id = ModelId{"nomic-embed-text"},
      .display_name = "nomic-embed-text",
      .capabilities = capability(Capability::Embedding),
      .context_length = 8192,
      .quality_score = 0.8f,
      .speed_score = 0.9f,
      .memory_mb = 1024,
      .cost_score = 0.1f,
  };
}

float preference_multiplier(RoutingPreference preference, const ModelInfo& candidate) {
  switch (preference) {
    case RoutingPreference::PreferCheap:
      return 1.0f / std::max(candidate.cost_score, 0.05f);
    case RoutingPreference::PreferQuality:
      return candidate.quality_score;
    case RoutingPreference::Balanced:
      return (candidate.quality_score + (1.0f / std::max(candidate.cost_score, 0.05f))) * 0.5f;
  }
  return 1.0f;
}

}  // namespace

ModelRegistry::ModelRegistry() { load_defaults(); }

ModelRegistry::ModelRegistry(std::vector<ModelInfo> models) : models_(std::move(models)) {}

void ModelRegistry::load_defaults() {
  models_ = {make_fast_code_model(), make_main_reasoner_model(), make_embed_model()};
}

std::optional<ModelInfo> ModelRegistry::find(const ModelId& id) const {
  for (const auto& model : models_) {
    if (model.id == id) {
      return model;
    }
  }
  return std::nullopt;
}

void ModelRegistry::add(ModelInfo model) { models_.push_back(std::move(model)); }

ModelRouter::ModelRouter(ModelRegistry registry) : registry_(std::move(registry)) {}

float ModelRouter::capability_match(Capabilities required, Capabilities offered) const {
  if (required == 0u) {
    return 1.0f;
  }

  uint32_t matched = 0;
  uint32_t count = 0;
  for (const auto cap :
       {Capability::Chat, Capability::Embedding, Capability::ToolUse, Capability::Vision,
        Capability::Code, Capability::Reasoning}) {
    if (!has_capability(required, cap)) {
      continue;
    }
    ++count;
    if (has_capability(offered, cap)) {
      ++matched;
    }
  }

  if (count == 0) {
    return 1.0f;
  }
  return static_cast<float>(matched) / static_cast<float>(count);
}

float ModelRouter::score_candidate(const ModelInfo& candidate,
                                   const RoutingRequest& request) const {
  const float match = capability_match(request.required_capabilities, candidate.capabilities);
  if (match <= 0.0f) {
    return 0.0f;
  }

  if (request.context_tokens > candidate.context_length) {
    return 0.0f;
  }

  if (request.estimated_memory_mb > 0 && candidate.memory_mb > request.estimated_memory_mb) {
    return 0.0f;
  }

  const float memory_divisor =
      request.estimated_memory_mb > 0
          ? static_cast<float>(std::max(request.estimated_memory_mb, 1u))
          : static_cast<float>(std::max(candidate.memory_mb, 1u));

  const float base =
      match * candidate.quality_score * candidate.speed_score /
      (static_cast<float>(candidate.memory_mb) / memory_divisor);

  return base * preference_multiplier(request.preference, candidate);
}

RoutingDecision ModelRouter::route(const RoutingRequest& request) const {
  RoutingDecision decision;
  float best_score = -1.0f;

  const auto required_quality = std::clamp(request.complexity, 0.0f, 1.0f);

  for (const auto& candidate : registry_.models()) {
    const float score = score_candidate(candidate, request);
    if (score <= 0.0f) {
      continue;
    }

    if (request.preference == RoutingPreference::PreferCheap &&
        candidate.quality_score + 0.05f < required_quality) {
      continue;
    }

    if (score > best_score) {
      best_score = score;
      decision.model = candidate.id;
      decision.score = score;
    }
  }

  if (decision.model.empty()) {
    decision.model = ModelId{"main-reasoner"};
    decision.score = 0.0f;
    decision.reason = "fallback to main-reasoner";
    return decision;
  }

  if (request.escalation_requested) {
    if (auto escalated = check_escalation(request, decision.model)) {
      return *escalated;
    }
  }

  std::ostringstream reason;
  reason << "selected " << decision.model.str() << " score=" << decision.score;
  decision.reason = reason.str();
  return decision;
}

std::optional<RoutingDecision> ModelRouter::check_escalation(
    const RoutingRequest& request, const ModelId& current_model, uint32_t retry_count) const {
  if (retry_count >= escalation_.max_retries && request.observed_quality >=
                                                   escalation_.quality_threshold &&
      request.complexity < escalation_.complexity_threshold) {
    return std::nullopt;
  }

  const bool low_quality = request.observed_quality < escalation_.quality_threshold;
  const bool high_complexity = request.complexity >= escalation_.complexity_threshold;
  const bool explicit_escalation = request.escalation_requested;

  if (!low_quality && !high_complexity && !explicit_escalation) {
    return std::nullopt;
  }

  const auto current = registry_.find(current_model);
  std::optional<ModelInfo> best;
  float best_quality = current ? current->quality_score : 0.0f;

  for (const auto& candidate : registry_.models()) {
    if (candidate.id == current_model) {
      continue;
    }
    if (score_candidate(candidate, request) <= 0.0f) {
      continue;
    }
    if (candidate.quality_score <= best_quality) {
      continue;
    }
    best_quality = candidate.quality_score;
    best = candidate;
  }

  if (!best) {
    return std::nullopt;
  }

  RoutingDecision decision;
  decision.model = best->id;
  decision.score = score_candidate(*best, request);
  decision.escalated = true;
  decision.reason = "escalated from " + current_model.str() + " to " + best->id.str();
  return decision;
}

TaskCategory ModelRouter::classify(std::string_view goal, AgentMode mode) const {
  return classify_task(goal, mode);
}

}  // namespace localagent
