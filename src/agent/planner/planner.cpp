#include "localagent/agent/planner/planner.hpp"

#include "localagent/common/strong_id.hpp"

#include <sstream>

namespace localagent {

namespace {

const char* status_label(PlanStepStatus status) {
  switch (status) {
    case PlanStepStatus::Pending:
      return "pending";
    case PlanStepStatus::InProgress:
      return "in_progress";
    case PlanStepStatus::Done:
      return "done";
    case PlanStepStatus::Skipped:
      return "skipped";
    case PlanStepStatus::Failed:
      return "failed";
  }
  return "pending";
}

}  // namespace

Plan Planner::create_plan(std::string goal, std::vector<std::string> step_titles) const {
  Plan plan;
  plan.goal = std::move(goal);
  for (auto& title : step_titles) {
    plan.steps.push_back(PlanStep{StepId{make_uuid()}, std::move(title), {}, PlanStepStatus::Pending});
  }
  return plan;
}

std::string Planner::render_markdown(const Plan& plan) const {
  std::ostringstream oss;
  oss << "# Plan\n\n";
  oss << "Goal: " << plan.goal << "\n\n";
  int index = 1;
  for (const auto& step : plan.steps) {
    oss << index++ << ". [" << status_label(step.status) << "] " << step.title;
    if (!step.detail.empty()) {
      oss << " — " << step.detail;
    }
    oss << "\n";
  }
  return oss.str();
}

void Planner::revise_step(Plan& plan, const StepId& id, PlanStepStatus status,
                          std::optional<std::string> detail) const {
  for (auto& step : plan.steps) {
    if (step.id == id) {
      step.status = status;
      if (detail) {
        step.detail = *detail;
      }
      return;
    }
  }
}

}  // namespace localagent
