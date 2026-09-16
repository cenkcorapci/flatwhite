#pragma once

#include "localagent/common/strong_id.hpp"

#include <optional>
#include <string>
#include <vector>

namespace localagent {

enum class PlanStepStatus { Pending, InProgress, Done, Skipped, Failed };

struct PlanStep {
  StepId id;
  std::string title;
  std::string detail;
  PlanStepStatus status{PlanStepStatus::Pending};
};

struct Plan {
  std::string goal;
  std::vector<PlanStep> steps;
};

class Planner {
public:
  [[nodiscard]] Plan create_plan(std::string goal, std::vector<std::string> step_titles) const;
  [[nodiscard]] std::string render_markdown(const Plan& plan) const;
  void revise_step(Plan& plan, const StepId& id, PlanStepStatus status,
                   std::optional<std::string> detail = std::nullopt) const;
};

}  // namespace localagent
