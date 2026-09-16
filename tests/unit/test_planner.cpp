#include "localagent/agent/planner/planner.hpp"

#include <catch2/catch_test_macros.hpp>

using namespace localagent;

TEST_CASE("Planner creates and renders markdown plan", "[planner]") {
  Planner planner;
  auto plan = planner.create_plan("ship feature", {"design", "implement", "verify"});
  REQUIRE(plan.steps.size() == 3);
  const auto md = planner.render_markdown(plan);
  CHECK(md.find("ship feature") != std::string::npos);
  CHECK(md.find("[pending] design") != std::string::npos);

  planner.revise_step(plan, plan.steps[1].id, PlanStepStatus::Done, "implemented");
  CHECK(plan.steps[1].status == PlanStepStatus::Done);
  CHECK(plan.steps[1].detail == "implemented");
}
