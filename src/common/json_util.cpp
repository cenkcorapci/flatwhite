#include "localagent/common/json_util.hpp"

#include <stdexcept>

namespace localagent {

nlohmann::json parse_or_throw(std::string_view text) {
  try {
    return nlohmann::json::parse(text.begin(), text.end());
  } catch (const nlohmann::json::parse_error& error) {
    throw std::runtime_error(std::string{"JSON parse error: "} + error.what());
  }
}

std::string dump_compact(const nlohmann::json& value) {
  return value.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string get_string_or(const nlohmann::json& value, std::string_view key,
                          std::string default_value) {
  if (!value.contains(key)) {
    return default_value;
  }
  const auto& entry = value.at(key);
  if (entry.is_string()) {
    return entry.get<std::string>();
  }
  if (entry.is_number_integer() || entry.is_number_unsigned()) {
    return std::to_string(entry.get<int64_t>());
  }
  if (entry.is_number_float()) {
    return std::to_string(entry.get<double>());
  }
  if (entry.is_boolean()) {
    return entry.get<bool>() ? "true" : "false";
  }
  return default_value;
}

std::optional<std::string> json_string(const nlohmann::json& j, std::string_view key) {
  if (!j.contains(key) || !j.at(key).is_string()) {
    return std::nullopt;
  }
  return j.at(key).get<std::string>();
}

std::optional<int> json_int(const nlohmann::json& j, std::string_view key) {
  if (!j.contains(key) || !j.at(key).is_number_integer()) {
    return std::nullopt;
  }
  return j.at(key).get<int>();
}

std::optional<bool> json_bool(const nlohmann::json& j, std::string_view key) {
  if (!j.contains(key) || !j.at(key).is_boolean()) {
    return std::nullopt;
  }
  return j.at(key).get<bool>();
}

}  // namespace localagent
