#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>

namespace localagent {

[[nodiscard]] nlohmann::json parse_or_throw(std::string_view text);
[[nodiscard]] std::string dump_compact(const nlohmann::json& value);
[[nodiscard]] std::string get_string_or(const nlohmann::json& value,
                                        std::string_view key,
                                        std::string default_value = {});

[[nodiscard]] std::optional<std::string> json_string(const nlohmann::json& j,
                                                     std::string_view key);
[[nodiscard]] std::optional<int> json_int(const nlohmann::json& j, std::string_view key);
[[nodiscard]] std::optional<bool> json_bool(const nlohmann::json& j, std::string_view key);

}  // namespace localagent
