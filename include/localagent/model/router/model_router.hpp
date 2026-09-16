#pragma once

#include <localagent/agent/types.hpp>
#include <localagent/model/model_types.hpp>

#include <optional>
#include <string>
#include <vector>

namespace localagent {

enum class RoutingPreference { PreferCheap, PreferQuality, Balanced };

struct RoutingRequest {
  AgentMode mode{AgentMode::Agent};
  TaskCategory category{TaskCategory::Conversation};
  float complexity{0.5f};
  Capabilities required_capabilities{capability(Capability::Chat)};
  uint32_t estimated_memory_mb{0};
  uint32_t context_tokens{0};
  RoutingPreference preference{RoutingPreference::Balanced};
  bool escalation_requested{false};
  std::optional<ModelId> current_model;
  float observed_quality{1.0f};
};

struct RoutingDecision {
  ModelId model;
  float score{0.0f};
  bool escalated{false};
  std::string reason;
};

struct EscalationTrigger {
  float quality_threshold{0.6f};
  float complexity_threshold{0.75f};
  uint32_t max_retries{1};
};

class ModelRegistry {
public:
  ModelRegistry();

  explicit ModelRegistry(std::vector<ModelInfo> models);

  [[nodiscard]] const std::vector<ModelInfo>& models() const noexcept { return models_; }

  [[nodiscard]] std::optional<ModelInfo> find(const ModelId& id) const;

  void add(ModelInfo model);

private:
  void load_defaults();

  std::vector<ModelInfo> models_;
};

class ModelRouter {
public:
  explicit ModelRouter(ModelRegistry registry = {});

  [[nodiscard]] RoutingDecision route(const RoutingRequest& request) const;

  [[nodiscard]] std::optional<RoutingDecision> check_escalation(
      const RoutingRequest& request, const ModelId& current_model,
      uint32_t retry_count = 0) const;

  [[nodiscard]] TaskCategory classify(std::string_view goal, AgentMode mode) const;

  [[nodiscard]] const ModelRegistry& registry() const noexcept { return registry_; }

private:
  [[nodiscard]] float score_candidate(const ModelInfo& candidate,
                                      const RoutingRequest& request) const;

  [[nodiscard]] float capability_match(Capabilities required,
                                       Capabilities offered) const;

  ModelRegistry registry_;
  EscalationTrigger escalation_;
};

}  // namespace localagent
