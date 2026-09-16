#pragma once

#include <string>

namespace localagent {

[[nodiscard]] std::string redact_secrets(std::string text);

}  // namespace localagent
