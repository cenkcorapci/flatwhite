#pragma once

#include "localagent/permissions/permission_engine.hpp"

#include <string>
#include <vector>

namespace localagent {

struct CommandClassification {
  RiskLevel primary_risk{RiskLevel::Read};
  std::vector<RiskLevel> risks;
  std::string summary;
};

[[nodiscard]] CommandClassification classify_command(const std::vector<std::string>& argv);

}  // namespace localagent
